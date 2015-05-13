DROP TABLE IF EXISTS projeto.players CASCADE;
DROP TABLE IF EXISTS projeto.games CASCADE;
DROP TABLE IF EXISTS projeto.questions CASCADE;
DROP TABLE IF EXISTS projeto.gamequestions CASCADE;
DROP TABLE IF EXISTS projeto.messages CASCADE;

CREATE TYPE statusEnum AS ENUM ('online', 'busy', 'offline');

CREATE TABLE projeto.players
(
	id BIGSERIAL PRIMARY KEY,
	name VARCHAR (20) UNIQUE NOT NULL,
	password VARCHAR (20) NOT NULL,
	admin BOOLEAN NOT NULL,
	online statusEnum NOT NULL,
	gamesWon INTEGER NOT NULL
);

CREATE TABLE projeto.games
(
	id BIGSERIAL PRIMARY KEY,
	creator_id BIGINT REFERENCES projeto.players (id),
	questions INTEGER NOT NULL
);

CREATE TABLE projeto.questions
(
	id BIGSERIAL PRIMARY KEY,
	question VARCHAR (30) NOT NULL,
	answer VARCHAR (30) NOT NULL,
	wrong1 VARCHAR (30) NOT NULL,
	wrong2 VARCHAR (30) NOT NULL,
	wrong3 VARCHAR (30) NOT NULL
);

CREATE TABLE projeto.gamequestions
(
	game_id BIGINT REFERENCES projeto.games (id),
	question_nr INTEGER NOT NULL,
	question_id BIGINT REFERENCES projeto.questions (id)
);

CREATE TABLE projeto.messages
(  
	sender_id BIGINT REFERENCES projeto.players (id) NOT NULL, 
	receiver_id BIGINT REFERENCES projeto.players (id) NOT NULL,
	msgText VARCHAR (30) NOT NULL
);