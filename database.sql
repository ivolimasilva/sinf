CREATE TABLE projeto.players
(
	id BIGSERIAL PRIMARY KEY,
	name VARCHAR (20) UNIQUE NOT NULL,
	password VARCHAR (20) NOT NULL,
	admin BOOLEAN,
	online BOOLEAN
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

CREATE TABLE projeto.gameplayers
(
	game_id BIGINT REFERENCES projeto.games (id),
	player_id BIGINT REFERENCES projeto.players (id)
);

CREATE TABLE projeto.gamequestions
(
	game_id BIGINT REFERENCES projeto.games (id),
	question_nr INTEGER NOT NULL,
	question_id BIGINT REFERENCES projeto.questions (id)
);