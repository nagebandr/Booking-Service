-- Schema for Event Seat Booking Service

CREATE TABLE IF NOT EXISTS users (
    id SERIAL PRIMARY KEY,
    name VARCHAR(100) NOT NULL,
    email VARCHAR(255) UNIQUE NOT NULL,
    created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW()
);

CREATE TABLE IF NOT EXISTS events (
    id SERIAL PRIMARY KEY,
    title VARCHAR(255) NOT NULL,
    description TEXT,
    venue VARCHAR(255) NOT NULL,
    start_time TIMESTAMP WITH TIME ZONE NOT NULL,
    total_seats INT NOT NULL DEFAULT 0,
    created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW()
);

CREATE TABLE IF NOT EXISTS seats (
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

CREATE TABLE IF NOT EXISTS bookings (
    id SERIAL PRIMARY KEY,
    event_id INT NOT NULL REFERENCES events(id) ON DELETE CASCADE,
    seat_id INT NOT NULL REFERENCES seats(id) ON DELETE CASCADE,
    user_id INT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    status VARCHAR(20) NOT NULL DEFAULT 'CONFIRMED' CHECK (status IN ('CONFIRMED', 'CANCELLED')),
    created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW()
);

-- Crucial unique partial index:
-- Guarantees at the storage engine level that a seat cannot have more than one CONFIRMED booking at any time!
CREATE UNIQUE INDEX IF NOT EXISTS idx_unique_confirmed_seat_booking 
ON bookings (seat_id) 
WHERE (status = 'CONFIRMED');

-- Indexes for performance
CREATE INDEX IF NOT EXISTS idx_seats_event_id ON seats(event_id);
CREATE INDEX IF NOT EXISTS idx_seats_event_booked ON seats(event_id, is_booked);
CREATE INDEX IF NOT EXISTS idx_bookings_user_id ON bookings(user_id);
CREATE INDEX IF NOT EXISTS idx_bookings_event_id ON bookings(event_id);

-- Seed initial test data
INSERT INTO users (name, email) VALUES
    ('Alice Johnson', 'alice@example.com'),
    ('Bob Smith', 'bob@example.com'),
    ('Charlie Brown', 'charlie@example.com')
ON CONFLICT (email) DO NOTHING;

INSERT INTO events (title, description, venue, start_time, total_seats) VALUES
    ('Hans Zimmer Live', 'Symphonic film music orchestra experience', 'Royal Albert Hall', NOW() + INTERVAL '30 days', 50),
    ('C++ Russia Conference', 'Annual high-performance C++ conference', 'Main Hall', NOW() + INTERVAL '60 days', 100)
ON CONFLICT DO NOTHING;

-- Populate seats for Event 1 (rows A-E, seats 1-10)
DO $$
DECLARE
    r RECORD;
    row_char TEXT;
    seat_num INT;
    ev_id INT;
BEGIN
    SELECT id INTO ev_id FROM events WHERE title = 'Hans Zimmer Live' LIMIT 1;
    IF ev_id IS NOT NULL THEN
        FOREACH row_char IN ARRAY ARRAY['A', 'B', 'C', 'D', 'E']
        LOOP
            FOR seat_num IN 1..10
            LOOP
                INSERT INTO seats (event_id, seat_row, seat_number, price, is_booked)
                VALUES (ev_id, row_char, seat_num, 75.00, FALSE)
                ON CONFLICT (event_id, seat_row, seat_number) DO NOTHING;
            END LOOP;
        END LOOP;
    END IF;
END $$;
