#include <iostream>		// cout
#include <sstream>		// cout
#include <stdlib.h>		// exit
#include <string.h>		// bzero
#include <unistd.h>		// close

#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>

#include <pthread.h>

#include <time.h>
#include <postgresql/libpq-fe.h>
#include <algorithm>

#include <set>
#include <map>

using namespace std;

PGconn* conn = NULL;

pthread_mutex_t mutex;
set <int> clients;
map <string, int> userSockets;

struct userAcc
{
	int id;
	string name;
	bool admin;
};

/* FUNÇOES DE ESCRITA ENTRE SOCKETS */

/* Envia uma string para um socket */
void writeline (int socketfd, string line)
{
	string tosend = line + "\n";
	write (socketfd, tosend.c_str(), tosend.length());
}

/* Lê uma linha de um socket; Retorna false se o socket se tiver fechado */
bool readline (int socketfd, string &line)
{
	int
		n; 

	char
		buffer[1025]; //buffer de tamanho 1025 para ter espaço para o \0 que indica o fim de string

	/* inicializar a string */
	line = "";

	// Enquanto não encontrarmos o fim de linha vamos lendo mais dados da stream
	while (line.find ('\n') == string::npos)
	{
		// leu n carateres. se for zero chegamos ao fim
		int n = read (socketfd, buffer, 1024); // ler do socket

		if (n == 0)
			return false; // nada para ser lido -> socket fechado

		buffer[n] = 0; // colocar o \0 no fim do buffer
		line += buffer; // acrescentar os dados lidos à string
	}

	// Retirar o \r\n (lemos uma linha mas não precisamos do \r\n)
	line.erase (line.end () - 1);
	line.erase (line.end () - 1);
	return true;  
}

/* Envia uma mensagem para todos os clientes ligados exceto 1 */
void broadcast (int origin, string text)
{
	/* Usamos um ostringstream para construir uma string
	Funciona como um cout mas em vez de imprimir no ecrã
	imprime numa string */
	ostringstream
		message;

	message << origin << " said: " << text;

	// Iterador para sets de inteiros 
	set<int>::iterator it;
	
	for (it = clients.begin (); it != clients.end (); it++)
	{
		if (*it != origin)
			writeline (*it, message.str ());
	}
}

/* FUNÇOES DE ESCRITA ENTRE SOCKETS */

void empty_curr_user (userAcc *currentUser)
{
	cout << endl << "Cleaning user!" << endl;
	(*currentUser).id = 0;
	(*currentUser).name = "";
	(*currentUser).admin = false;
}

void print_curr_user (userAcc *currentUser)
{
	cout << endl << "User mem: " << currentUser << endl;
	cout << "ID: " << (*currentUser).id << endl;
	cout << "Name: " << (*currentUser).name << endl;
	cout << "Admin: " << (*currentUser).admin << endl << endl;
}

/* FUNÇOES DE CONTACTO COM A BASE-DE-DADOS */

PGresult* executeSQL(string sql)
{
	PGresult* res = PQexec(conn, sql.c_str());

	if (!(PQresultStatus (res) == PGRES_COMMAND_OK || PQresultStatus (res) == PGRES_TUPLES_OK))
	{
		cout << "Não foi possível executar o comando: >" << endl << sql << "<" << endl << ">" << sql.c_str() << "<" << endl << " = " << PQresultStatus (res) << endl;
		return NULL;
	}

	return res;
}

void initDB()
{
	conn = PQconnectdb ("host='vdbm.fe.up.pt' user='sinf15g34' password='eu' dbname='sinf15g34'");

	if (!conn)
	{
		cout << "Não foi possivel ligar a base de dados 1" << endl;
		exit(-1);
	}

	if (PQstatus(conn) != CONNECTION_OK)
	{
		cout << "Não foi possivel ligar a base de dados 2" << endl;
		exit(-1);
	}
	else
	{
		executeSQL ("SET SCHEMA 'projeto'");
	}
}

void closeDB ()
{
	PQfinish (conn);
}

/* FUNÇOES DE CONTACTO COM A BASE-DE-DADOS */

/* FUNÇOES DO JOGO */

void cmd_help (int socketfd)
{
	ostringstream
		oss;
	string
		data;

	oss << "Pode usar umas das seguintes funções:" << endl
	<< "-------------------------------------" << endl
	<< "\\help" << endl
	<< "	Lista os comandos todos disponiveis;" << endl
	<< "\\register <name> <password>" << endl
	<< "	Regista um user com name e password;" << endl
	<< "\\login <name> <password>" << endl
	<< "	Login do user com a sua password;" << endl
	<< "\\logout" << endl
	<< "	Faz logout da sessão actual;" << endl
	<< "\\question <question> <right_answer> <wrong_answer> <wrong_answer> <wrong_answer>"<< endl
	<< "	Cria uma questao sem ser atribuida a um jogo;" << endl
	<< "\\create <timer>" << endl
	<< "	Cria um jogo (o timer nao funciona);" << endl
	<< "\\insert <game_id> <question_id>" << endl
	<< "	Associa uma questao a um jogo;" << endl
	<< "\\start <game_id>" << endl
	<< "	Comeca o jogo com o ID indicado;" << endl
	<< "\\ranking" << endl
	<< "	Lista todos os utilizadores por ranking;" << endl
	<< "\\listusers" << endl
	<< "	Lista todos os users registados;" << endl
	<< "\\exit" << endl
	<< "	Desliga-se do servidor;" << endl;

	data = oss.str();
	writeline (socketfd, data);
}

void cmd_register (int socketfd, string &line)
{
	string
		comando,
		user,
		pass;
	istringstream
		iss(line);
 
	iss >> comando >> user >> pass;
	
	if (pass.size() < 4)
	{
		writeline (socketfd, "Palavra-passe muito pequena!");
		return ;
	}
	else
	{	
		PGresult* res = executeSQL ("SELECT * FROM players WHERE name = '" + user + "'");
		
		if (PQntuples (res) > 0) // caso já existe um utilizador com o mesmo username 
		{
			writeline (socketfd, "Já existe um utilizador com este nome!");
		}
		else // caso não exista este username
		{
			executeSQL ("INSERT INTO players (name, password, admin, online, gameswon) VALUES ('" + user + "', '" + pass + "', FALSE, 'offline', 0)"); //não é admin nem está online
		}
	}
}

void cmd_login (userAcc *currentUser, int socketfd, string &line)
{
	string
		comando,
		user,
		pass;
	istringstream
		iss(line);
 
	iss >> comando >> user >> pass;
	
	if ((*currentUser).id != 0)
	{
		writeline (socketfd, "Já existe uma sessão iniciada. Faça \\logout");
		return ;
	}

	PGresult* res = executeSQL ("SELECT * FROM players WHERE name = '" + user + "' AND password = '" + pass + "' AND status = 'offline'");
	
	if (PQntuples (res) == 1) // sucesso 
	{
		(*currentUser).id = atoi (PQgetvalue (res, 0, 0));
		(*currentUser).name = PQgetvalue (res, 0, 1);
		if (strcmp (PQgetvalue (res, 0, 3), "TRUE") == 0)
			(*currentUser).admin = true;
		else
			(*currentUser).admin = false;

		// print_curr_user ();
		
		ostringstream
			oss;

		oss << "UPDATE players SET status = 'online' WHERE id = " << (*currentUser).id;

		executeSQL (oss.str());

		userSockets[(*currentUser).name] = socketfd;
		
		writeline (socketfd, "Login com sucesso!");
	}
	else // insucesso
	{
		writeline (socketfd, "Erro: username/password errados ou conta já está aberta noutro client.");
	}
}

void cmd_logout (userAcc *currentUser, int socketfd)
{
	ostringstream
		oss;

	if ((*currentUser).id == 0)
	{
		writeline (socketfd, "Não existe uma sessão iniciada. Faça \\login");
		return ;
	}

	oss << "UPDATE players SET status = 'offline' WHERE id = " << (*currentUser).id;

	executeSQL (oss.str());
	
	empty_curr_user (currentUser);
	writeline (socketfd, "Logout com sucesso!");
}

void cmd_question (userAcc *currentUser, int socketfd, string &line)
{
	string
		comando,
		question,
		answer,
		wrong1,
		wrong2,
		wrong3,
		data;
	istringstream
		iss (line);
	ostringstream
		oss;
	
	if ((*currentUser).id == 0)
	{
		writeline (socketfd, "Não existe uma sessão iniciada. Faça \\login");
		return ;
	}
 
	iss >> comando >> question >> answer >> wrong1 >> wrong2 >> wrong3;
	
	PGresult* res = executeSQL ("SELECT * FROM questions WHERE question = '" + question + "'");
		
	if (PQntuples (res) > 0) // caso já existe esta questao 
	{
		writeline (socketfd, "Já existe esta pergunta!");
	}
	else // caso não exista esta questao
	{
		executeSQL ("INSERT INTO questions (question, answer, wrong1, wrong2, wrong3) VALUES ('" + question + "', '" + answer + "', '" + wrong1 + "', '" + wrong2 + "', '" + wrong3 + "')");
		res = executeSQL ("SELECT id FROM questions WHERE question = '" + question + "'");

		oss << "Questao criada com o ID: " << PQgetvalue (res, 0, 0);
	
		data = oss.str();
		writeline (socketfd, data);
	}
}

void cmd_create (userAcc *currentUser, int socketfd, string &line)
{
	string
		comando,
		time,
		data;
	istringstream
		iss (line);
	
	iss >> comando >> time;
	
	ostringstream
		oss1,
		oss2,
		oss3;

	if ((*currentUser).id == 0)
	{
		writeline (socketfd, "Não existe uma sessão iniciada. Faça \\login");
		return ;
	}

	oss1 << "INSERT INTO games (creator_id, questions, time) VALUES (" << (*currentUser).id << ", 0, " << time << ")";
	executeSQL (oss1.str());
	
	oss2 << "SELECT id FROM games WHERE creator_id = " << (*currentUser).id << " AND questions = 0 AND time = " << time << "";	
	PGresult* res = executeSQL (oss2.str());
	oss3 << "Jogo criado com o ID: " << PQgetvalue (res, 0, 0);
	
	data = oss3.str();
	writeline (socketfd, data);
}

void cmd_insert (userAcc *currentUser, int socketfd, string &line)
{
	string
		comando,
		game_id,
		question_id;
	istringstream
		iss (line);
	ostringstream
		oss1,
		oss2;
	PGresult*
		res;
	int
		question_nr;

	if ((*currentUser).id == 0)
	{
		writeline (socketfd, "Não existe uma sessão iniciada. Faça \\login");
		return ;
	}
	
	iss >> comando >> game_id >> question_id;
	
	res = executeSQL ("SELECT * FROM questions WHERE id = " + question_id + "");
	if (PQntuples (res) == 0)
	{
		writeline (socketfd, "Pergunta nao encontrado.");
		return ;
	}
	
	res = executeSQL ("SELECT questions FROM games WHERE id = " + game_id + "");
	if (PQntuples (res) == 0)
	{
		writeline (socketfd, "Jogo nao encontrado.");
		return ;
	}
	else
	{
		question_nr = 1 + atoi (PQgetvalue (res, 0, 0));
		oss1 << "INSERT INTO gamequestions (game_id, question_nr, question_id) VALUES (" << game_id << ", " << question_nr << ", " << question_id << ")";
		executeSQL (oss1.str ());
		
		oss2 << "UPDATE games SET questions = " << question_nr << " WHERE id = " << game_id;
		executeSQL (oss2.str ());
	}
}

void cmd_start (userAcc *currentUser, int socketfd, string &line)
{
	string
		comando,
		game_id,
		question_id,
		question,
		answer,
		answer1,
		answer2,
		answer3,
		answer4,
		line2,
		option;
	istringstream
		iss (line);
	ostringstream
		oss,
		oss1,
		oss2,
		oss3,
		ossUpdate1,
		ossUpdate2,
		ossUpdate3;
	int
		questions,
		waittime,
		i;
	PGresult*
		res;
	
	if ((*currentUser).id == 0)
	{
		writeline (socketfd, "Não existe uma sessão iniciada. Faça \\login");
		return ;
	}
	
	iss >> comando >> game_id;
	
	res = executeSQL ("SELECT questions, time FROM games WHERE id = " + game_id + "");
	if (PQntuples (res) == 0)
	{
		writeline (socketfd, "Jogo nao encontrado.");
		return ;
	}
	questions = atoi (PQgetvalue (res, 0, 0));
	waittime = atoi (PQgetvalue (res, 0, 1));

	oss3 << "Este jogo tem " << questions << " perguntas.";
	writeline (socketfd, oss3.str ());
	
	// meter o jogador a ocupado
	ossUpdate1 << "UPDATE players SET status = 'busy' WHERE id = " << (*currentUser).id;
	executeSQL (ossUpdate1.str());
	
	for (i = 0; i < questions; i++)
	{
		oss1.str ("");
		oss1.clear ();
		oss1 << "SELECT question_id FROM gamequestions WHERE game_id = " << game_id << " AND question_nr = " << i + 1;
		res = executeSQL (oss1.str ());
		question_id = PQgetvalue (res, 0, 0);

		oss2.str ("");
		oss2.clear ();
		oss2 << "SELECT question, answer, wrong1, wrong2, wrong3 FROM questions WHERE id = " << question_id;
		res = executeSQL (oss2.str ());
		
		srand (time (NULL));

		int
	        randomset[4] = {1, 2, 3, 4};
	       
	    random_shuffle (&randomset[0], &randomset[3]);

		question = PQgetvalue (res, 0, 0);
		answer = PQgetvalue (res, 0, 1);
		answer1 = PQgetvalue (res, 0, randomset[0]);
		answer2 = PQgetvalue (res, 0, randomset[1]);
		answer3 = PQgetvalue (res, 0, randomset[2]);
		answer4 = PQgetvalue (res, 0, randomset[3]);

		oss.str ("");
		oss.clear ();
		oss << "Pergunta: " << question << endl
			<< "Respostas:" << endl
			<< "a) " << answer1 << endl
			<< "b) " << answer2 << endl
			<< "c) " << answer3 << endl
			<< "d) " << answer4 << endl
			<< "Resposta:";
		
		writeline (socketfd, oss.str ());
		
		readline (socketfd, line2);

		istringstream
			iss1 (line2);
		iss1 >> option;
		
		if (option == "a" && answer1 == answer)
				writeline (socketfd, "Acertou! Próxima pergunta..");
		else if (option == "b" && answer2 == answer)
				writeline (socketfd, "Acertou! Próxima pergunta..");
		else if (option == "c" && answer3 == answer)
				writeline (socketfd, "Acertou! Próxima pergunta..");
		else if (option == "d" && answer4 == answer)
				writeline (socketfd, "Acertou! Próxima pergunta..");
		else
		{
			writeline (socketfd, "Perdeu! A sair do jogo..");
			break;
		}
	}
	
	if (i == questions)
	{
		writeline (socketfd, "Ganhou o jogo!");
		ossUpdate2 << "UPDATE players SET gameswon = gameswon + 1 WHERE id = " << (*currentUser).id;
		executeSQL (ossUpdate2.str());
	}
	
	// tira o jogador do ocupado
	ossUpdate3 << "UPDATE players SET status = 'online' WHERE id = " << (*currentUser).id;
	executeSQL (ossUpdate3.str());
}

void cmd_msg (userAcc *currentUser, int socketfd, string &line)
{
	if ((*currentUser).id == 0)
	{
		writeline (socketfd, "Não existe uma sessão iniciada. Faça \\login");
		return ;
	}
	
	istringstream
		iss (line);
	string
		comand,
		receiverName,
		text;
	ostringstream
		message,
		output;
	
	iss >> comand >> receiverName >> text;
	
	message << "Nova mensagem!" << endl << "De: " << (*currentUser).name << endl << "Mensagem: " << text;
	
	PGresult*
		res;
	
	res = executeSQL ("SELECT id FROM players WHERE name = '" + receiverName + "' AND status = 'online'");
	
	if (PQntuples (res) == 1)
	{	
		writeline (userSockets[receiverName], message.str ());

		output << "INSERT INTO messages (sender_id, receiver_id, msgtext, checked) VALUES (" << (*currentUser).id << ", " << PQgetvalue (res, 0, 0) << ", '" << text << "', TRUE)";
		
		executeSQL (output.str ());
	}
	else
	{
		res = executeSQL ("SELECT id FROM players WHERE name = '" + receiverName + "'");
		output << "INSERT INTO messages (sender_id, receiver_id, msgtext, checked) VALUES (" << (*currentUser).id << ", " << PQgetvalue (res, 0, 0) << ", '" << text << "', FALSE)";
		
		executeSQL (output.str ());
	}
}

void cmd_inbox (userAcc *currentUser, int socketfd)
{
	if ((*currentUser).id == 0)
	{
		writeline (socketfd, "Não existe uma sessão iniciada. Faça \\login");
		return ;
	}
	
	ostringstream
		oss,
		oss1,
		oss2;
	string
		data;
	PGresult* res;
	PGresult* res1;
	
	oss1 << "SELECT sender_id, msgtext FROM messages WHERE receiver_id = " << (*currentUser).id;
	res = executeSQL (oss1.str ());

	for (int i = 0; i < PQntuples (res); i++)
	{
		oss1.str ("");
		oss1.clear ();

		oss1 << "SELECT name FROM players WHERE id = " << PQgetvalue (res, i, 0);
		res1 = executeSQL (oss1.str ());
		oss << "De: " << PQgetvalue (res1, 0, 0) << endl << "Mensagem: " << PQgetvalue (res, i, 1) << endl << endl;
	}

	data = oss.str();
	writeline (socketfd, data);
	
	oss2 << "UPDATE messages SET checked = true WHERE receiver_id = " << (*currentUser).id;
	executeSQL (oss2.str ());
}

void cmd_outbox (userAcc *currentUser, int socketfd)
{
	if ((*currentUser).id == 0)
	{
		writeline (socketfd, "Não existe uma sessão iniciada. Faça \\login");
		return ;
	}
	
	ostringstream
		oss,
		oss1,
		oss2;
	string
		data;
	PGresult* res;
	PGresult* res1;
	
	oss1 << "SELECT receiver_id, msgtext FROM messages WHERE sender_id = " << (*currentUser).id;
	res = executeSQL (oss1.str ());

	for (int i = 0; i < PQntuples (res); i++)
	{
		oss1.str ("");
		oss1.clear ();
		
		oss1 << "SELECT name FROM players WHERE id = " << PQgetvalue (res, i, 0);
		res1 = executeSQL (oss1.str ());
		oss << "Para: " << PQgetvalue (res1, 0, 0) << endl << "Mensagem: " << PQgetvalue (res, i, 1) << endl << endl;
	}

	data = oss.str();
	writeline (socketfd, data);
}

void cmd_listusers (int socketfd)
{
	ostringstream
		oss;
	string
		data;
	
	PGresult* res = executeSQL ("SELECT * FROM players");
	for (int i = 0; i < PQntuples (res); i++)
	{
		oss << "ID: " << PQgetvalue (res, i, 0) << " | Nome: " << PQgetvalue (res, i, 1) << endl;
	}

	data = oss.str();
	writeline (socketfd, data);
}

void cmd_ranking (int socketfd)
{
	ostringstream
		oss;
	string
		data;
	
	PGresult* res = executeSQL ("SELECT name, gameswon FROM players ORDER BY gameswon DESC");
	for (int i = 0; i < PQntuples (res); i++)
	{
		oss << "#" << i + 1 << " | Nome: " << PQgetvalue (res, i, 0) << " (" << PQgetvalue (res, i, 1) << ")" << endl;
	}

	data = oss.str();
	writeline (socketfd, data);
}

/* FUNÇOES DO JOGO */

void verify_msg (userAcc *currentUser, int socketfd)
{
	ostringstream
		oss,
		oss1;

	oss << "SELECT * FROM messages WHERE receiver_id = " << (*currentUser).id << "AND checked = FALSE";
	PGresult* res = executeSQL (oss.str ());
	
	if (PQntuples (res) > 0)
	{
		oss1 << "Tem " << PQntuples (res) << " mensagens novas. Consulte \\inbox";
		writeline (socketfd, oss1.str ());
	}
}

/* Trata de receber dados de um cliente cujo socketid foi passado como parâmetro */
void* cliente (void* args)
{
	int
		socketfd = *(int*) args;
	string
		line;
	userAcc
		*currentUser = new userAcc();
	
	// empty_curr_user (currentUser);
	// print_curr_user (currentUser);
	
	cout << "Connecting client: " << socketfd << endl;

	pthread_mutex_lock(&mutex);
		clients.insert (socketfd);		
	pthread_mutex_unlock(&mutex);

	cout << "Client connected: " << socketfd << endl;

	writeline (socketfd, (*currentUser).name + "> ");
	while (readline (socketfd, line))
	{
		if (line.find ("\\help") == 0)
			cmd_help (socketfd);
		else if (line.find ("\\register") == 0)
			cmd_register (socketfd, line);
		else if (line.find ("\\login") == 0)
			cmd_login (currentUser, socketfd, line);
		else if (line.find ("\\logout") == 0)
			cmd_logout (currentUser, socketfd);
		else if (line.find ("\\question") == 0)
			cmd_question (currentUser, socketfd, line);
		else if (line.find ("\\create") == 0)
			cmd_create (currentUser, socketfd, line);
		else if (line.find ("\\insert") == 0)
			cmd_insert (currentUser, socketfd, line);
		else if (line.find ("\\start") == 0)
			cmd_start (currentUser, socketfd, line);
		else if (line.find ("\\msg") == 0)
			cmd_msg (currentUser, socketfd, line);
		else if (line.find ("\\inbox") == 0)
			cmd_inbox (currentUser, socketfd);
		else if (line.find ("\\outbox") == 0)
			cmd_outbox (currentUser, socketfd);
		else if (line.find ("\\listusers") == 0)
			cmd_listusers (socketfd);
		else if (line.find ("\\ranking") == 0)
			cmd_ranking (socketfd);
		else if (line.find ("\\exit") == 0)
		{
			cmd_logout (currentUser, socketfd);
			break;
		}

		if ((*currentUser).id != 0)
			verify_msg (currentUser, socketfd);

		writeline (socketfd, (*currentUser).name + "> ");
	}
	
	empty_curr_user (currentUser);

	cout << "Client disconnected: " << socketfd << endl;
	clients.erase (socketfd);

	// Fecha o socket
	close (socketfd);
}

int main (int argc, char *argv[])
{
	// Estruturas de dados
	int
		socketfd,
		newsocketfd,
		port = atoi (argv[1]);
	socklen_t
		client_addr_length;
	struct sockaddr_in
		serv_addr,
		cli_addr;

	cout << "Port: " << port << endl;

	initDB ();
	pthread_mutex_init(&mutex, NULL);

	// Inicializar o socket
	// AF_INET:			para indicar que queremos usar IP
	// SOCK_STREAM:		para indicar que queremos usar TCP
	// socketfd:			id do socket principal do servidor
	// Se retornar < 0 ocorreu um erro
	socketfd = socket (AF_INET, SOCK_STREAM, 0);
	if (socketfd < 0)
	{
		cout << "Error creating socket" << endl;
		exit(-1);
	}

	// Criar a estrutura que guarda o endereço do servidor
	// bzero:		apaga todos os dados da estrutura (coloca a 0's)
	// AF_INET:		endereço IP
	// INADDR_ANY:	aceitar pedidos para qualquer IP
	bzero ((char *) &serv_addr, sizeof (serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons (port);

	// Fazer bind do socket. Apenas nesta altura é que o socket fica ativo mas ainda não estamos a tentar receber ligações.
	// Se retornar < 0 ocorreu um erro
	int res = bind (socketfd, (struct sockaddr *) &serv_addr, sizeof (serv_addr));
	if (res < 0)
	{
		cout << "Error binding to socket" << endl;
		exit(-1);
	}

	// Indicar que queremos escutar no socket com um backlog de 5 (podem ficar até 5 ligações pendentes antes de fazermos accept)
	listen (socketfd, 5);

	while (true)
	{
		// Aceitar uma nova ligação. O endereço do cliente fica guardado em 
		// cli_addr:	endereço do cliente
		// newsocketfd:	id do socket que comunica com este cliente */
		client_addr_length = sizeof (cli_addr);
		newsocketfd = accept (socketfd, (struct sockaddr *) &cli_addr, &client_addr_length);
		
		// Criar uma thread para tratar dos pedidos do novo cliente
		pthread_t thread;
		pthread_create (&thread, NULL, cliente, &newsocketfd);
	}

	closeDB ();
	pthread_mutex_destroy(&mutex);
	close (socketfd);
	return 0; 
}