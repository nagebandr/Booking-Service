# Event Seat Booking Service (C++20 / Boost.Beast / PostgreSQL)

Высокопроизводительный асинхронный сервис бронирования мест на мероприятия, написанный на современном **C++20** с использованием **Boost.Asio**, **Boost.Beast**, **PostgreSQL (libpqxx)**, **GoogleTest** и **Google Benchmark**.

---

## Архитектура решения и обработка гонок (Race Conditions)

### Проблема одновременного бронирования

```text
User A ──┐
         ├──> Seat #42  (Оба одновременно отправляют POST /events/1/bookings)
User B ──┘
```

В классических наивных реализациях (без блокировок) происходит **Check-Then-Act race condition**:
1. User A читает статус: место свободно (`is_booked = false`).
2. User B читает статус: место свободно (`is_booked = false`).
3. User A создает запись бронирования и ставит `is_booked = true`.
4. User B перезаписывает бронирование и тоже ставит `is_booked = true`.
5. **Итог**: Одно и то же физическое место продано дважды (**Double Booking**).

---

### Многоуровневая защита от гонок в PostgreSQL

Данный сервис реализует **двухуровневую модель защиты** (Two-Tier Defense):

#### 1. Строковая блокировка на уровне транзакции (`SELECT ... FOR UPDATE`)
При обработке запроса бронирования выполняется транзакция (`pqxx::work txn`):

```sql
BEGIN;

-- 1. Блокируем конкретную строку выбранного места на уровне хранилища PostgreSQL:
SELECT id, event_id, is_booked 
FROM seats 
WHERE id = $1 AND event_id = $2 
FOR UPDATE;

-- 2. Если is_booked == true -> откат и возврат 409 Conflict.

-- 3. Если свободно -> обновляем и создаем запись:
UPDATE seats SET is_booked = TRUE, version = version + 1 WHERE id = $1;
INSERT INTO bookings (event_id, seat_id, user_id, status) VALUES ($1, $2, $3, 'CONFIRMED');

COMMIT;
```

**Как это работает при конкурентных запросах:**
* Когда User A и User B одновременно вызывают этот запрос, один из них (например, User A) первым захватывает **эксклюзивную блокировку строки** в PostgreSQL.
* Транзакция User B **приостанавливается** на этапе `SELECT ... FOR UPDATE` и ждет освобождения блокировки.
* User A успешно фиксирует (`COMMIT`) транзакцию: место помечено как забронированное.
* Транзакция User B разблокируется. В режиме изоляции `READ COMMITTED` (дефолт в PostgreSQL) запрос `FOR UPDATE` повторно считывает свежую версию строки, видит `is_booked = true`.
* Код C++ немедленно определяет конфликт и возвращает клиенту **HTTP 409 Conflict** (`"Seat is already booked"`).

#### 2. Гарантия целостности на уровне движка БД (Partial Unique Index)
Даже если в приложении возникнет логическая ошибка или будет выполнен сырой SQL-скрипт в обход блокировки строки, на уровне PostgreSQL создан уникальный частичный индекс:

```sql
CREATE UNIQUE INDEX idx_unique_confirmed_seat_booking 
ON bookings (seat_id) 
WHERE (status = 'CONFIRMED');
```
Он делает физически невозможным существование двух активных подтвержденных бронирований для одного и того же места. Попытка вставки вызовет `unique_violation` (код ошибки `23505`), который перехватывается сервером и транслируется в HTTP 409 Conflict.

---

## Архитектура C++ Backend

* **C++20**: Использование `std::barrier`, `std::jthread` / `std::thread`, concept-friendly STL, `std::string_view`, обозначений `[[nodiscard]]`.
* **Boost.Asio & Boost.Beast**:
  * Неблокирующий HTTP-сервер на базе пула потоков (`io_context`).
  * Асинхронное чтение и запись с поддержкой HTTP Keep-Alive и ограничениями по таймаутам.
  * Strand-модель диспетчеризации сокетов для предотвращения race condition на уровне сокетов.
* **Connection Pool**:
  * Собственный потокобезопасный пул соединений к PostgreSQL на `std::mutex` и `std::condition_variable`.
  * RAII-обертка `PooledConnection`, автоматически возвращающая соединение в пул по выходу из области видимости.
* **Тестирование**:
  * `GoogleTest`: юнит-тесты сериализации/конфигурации и **многопоточный интеграционный стресс-тест на 20 одновременных потоков** с `std::barrier`.
  * `Google Benchmark`: замеры пропускной способности сериализации и парсинга.

---

## Структура проекта

```text
booking_service/
├── CMakeLists.txt            # Сборка проекта, модули Boost, pqxx, GTest, GBenchmark
├── Dockerfile                # Multi-stage сборка (Ubuntu 24.04, GCC 13/14, Ninja)
├── docker-compose.yml        # PostgreSQL 16 + C++ Backend + Test runner
├── .clang-format             # Стандарты форматирования Google / LLVM
├── .clang-tidy               # Правила статического анализа Modern C++
├── sql/
│   └── 01_init.sql           # Схема БД, индексы, ограничения и тестовые данные
├── include/
│   ├── config.hpp            # Парсинг настроек из переменных окружения
│   ├── db/
│   │   ├── connection_pool.hpp    # Потокобезопасный пул соединений
│   │   └── booking_repository.hpp # Логика БД и блокировок
│   ├── models/               # DTO модели (User, Event, Seat, Booking)
│   └── http/
│       ├── http_server.hpp   # Слушатель TCP соединений Boost.Asio
│       ├── http_session.hpp  # Асинхронная HTTP-сессия Boost.Beast
│       ├── router.hpp        # Роутер REST эндпоинтов
│       └── json_utils.hpp    # Утилиты генерации HTTP/JSON ответов
├── src/                      # Реализация компонентов
├── tests/
│   ├── unit_tests.cpp        # Юнит-тесты моделей и конфигурации
│   └── concurrency_tests.cpp # Стресс-тест конкурентного бронирования места #42
└── benchmarks/
    └── booking_benchmark.cpp # Бенчмарки Google Benchmark
```

---

## Схема базы данных

```sql
users (
    id SERIAL PRIMARY KEY,
    name VARCHAR(100) NOT NULL,
    email VARCHAR(255) UNIQUE NOT NULL,
    created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW()
);

events (
    id SERIAL PRIMARY KEY,
    title VARCHAR(255) NOT NULL,
    description TEXT,
    venue VARCHAR(255) NOT NULL,
    start_time TIMESTAMP WITH TIME ZONE NOT NULL,
    total_seats INT NOT NULL DEFAULT 0,
    created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW()
);

seats (
    id SERIAL PRIMARY KEY,
    event_id INT NOT NULL REFERENCES events(id) ON DELETE CASCADE,
    seat_row VARCHAR(10) NOT NULL,
    seat_number INT NOT NULL,
    price NUMERIC(10, 2) NOT NULL DEFAULT 0.00,
    is_booked BOOLEAN NOT NULL DEFAULT FALSE,
    version INT NOT NULL DEFAULT 0,
    created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    CONSTRAINT uq_event_row_number UNIQUE (event_id, seat_row, seat_number)
);

bookings (
    id SERIAL PRIMARY KEY,
    event_id INT NOT NULL REFERENCES events(id) ON DELETE CASCADE,
    seat_id INT NOT NULL REFERENCES seats(id) ON DELETE CASCADE,
    user_id INT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    status VARCHAR(20) NOT NULL DEFAULT 'CONFIRMED' CHECK (status IN ('CONFIRMED', 'CANCELLED')),
    created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW()
);
```

---

## REST API спецификация

### 1. Проверка работоспособности
* **GET** `/health`
* **Response** `200 OK`:
  ```json
  {
    "status": "ok",
    "database": "connected"
  }
  ```

### 2. Список мероприятий
* **GET** `/events`
* **Response** `200 OK`:
  ```json
  [
    {
      "id": 1,
      "title": "Hans Zimmer Live",
      "description": "Symphonic film music orchestra experience",
      "venue": "Royal Albert Hall",
      "start_time": "2026-10-30T17:19:28Z",
      "total_seats": 50,
      "created_at": "2026-09-30T17:19:28Z"
    }
  ]
  ```

### 3. Список мест мероприятия
* **GET** `/events/{id}/seats`
* **Response** `200 OK`:
  ```json
  [
    {
      "id": 1,
      "event_id": 1,
      "seat_row": "A",
      "seat_number": 1,
      "price": 75.0,
      "is_booked": false,
      "version": 0,
      "created_at": "2026-09-30T17:19:28Z"
    }
  ]
  ```

### 4. Бронирование места (Атомарная операция)
* **POST** `/events/{id}/bookings`
* **Request Body**:
  ```json
  {
    "user_id": 1,
    "seat_id": 42
  }
  ```
* **Response (Успех)** `201 Created`:
  ```json
  {
    "message": "Seat successfully booked",
    "booking": {
      "id": 1,
      "event_id": 1,
      "seat_id": 42,
      "user_id": 1,
      "status": "CONFIRMED",
      "created_at": "2026-09-30T17:25:00Z"
    }
  }
  ```
* **Response (Место уже занято)** `409 Conflict`:
  ```json
  {
    "error": "Seat is already booked",
    "code": 409
  }
  ```

### 5. Отмена бронирования
* **DELETE** `/bookings/{id}`
* **Response (Успех)** `200 OK`:
  ```json
  {
    "message": "Booking cancelled successfully",
    "booking_id": 1
  }
  ```

---

## Запуск и тестирование через Docker Compose

### 1. Запуск сервиса и базы данных
```bash
docker-compose up --build -d
```
Сервис станет доступен по адресу `http://localhost:8080`.

### 2. Запуск интеграционных тестов конкурентности (GoogleTest)
```bash
docker-compose --profile test run --rm tests
```
Тест запускает `concurrency_tests.cpp`, где `std::barrier` синхронизирует 20 одновременных потоков, бьющих в одно и то же место #42:
* Ровно 1 запрос получает статус успешного бронирования (`Success / 201`).
* Все остальные 19 запросов получают статус конфликта (`Conflict / 409`).
* Проверяется отсутствие повреждения состояния БД.

### 3. Запуск бенчмарков (Google Benchmark)
```bash
docker-compose run --rm booking-service /usr/local/bin/booking_benchmarks
```

---

## Тестирование гонки вручную (cURL)

В терминале Bash / PowerShell можно одновременно запустить два параллельных запроса на бронирование места `id=5`:

```bash
# User A (id: 1)
curl -X POST http://localhost:8080/events/1/bookings \
  -H "Content-Type: application/json" \
  -d '{"user_id": 1, "seat_id": 5}' &

# User B (id: 2)
curl -X POST http://localhost:8080/events/1/bookings \
  -H "Content-Type: application/json" \
  -d '{"user_id": 2, "seat_id": 5}' &

wait
```

Один запрос вернет `201 Created`, а второй гарантированно вернет `409 Conflict`.
