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

#include <set>

using namespace std;

PGconn* conn = NULL;

set <int> clients;

/* FUNÇOES DE ESCRITA ENTRE SOCKETS */

/* Envia uma string para um socket */
void writeline (int socketfd, string line)
{
	string tosend = line + "\n";
	write(socketfd, tosend.c_str(), tosend.length());
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

/* FUNÇOES DE CONTACTO COM A BASE-DE-DADOS */

PGresult* executeSQL(string sql)
{
	PGresult* res = PQexec(conn, sql.c_str());
	
	if (!(PQresultStatus(res) == PGRES_COMMAND_OK || PQresultStatus(res) == PGRES_TUPLES_OK))
	{
		cout << "Não foi possí­vel executar o comando!" << endl;
		return NULL;
	}
	
	return res;
}

void initDB ()
{
	conn = PQconnectdb("host='vdbm.fe.up.pt' user='sinf15g34' password='eu' dbname='sinf15g34'");
	
	if (!conn)
	{
		cout << "Não foi possivel ligar a base de dados" << endl;
		exit(-1);
	}
	
	if (PQstatus(conn) != CONNECTION_OK)
	{
		cout << "Não foi possivel ligar a base de dados" << endl;
		exit(-1);
	}
	else
	{
		executeSQL ("SET SCHEMA 'projeto'");
	}
}

void closeDB ()
{
	PQfinish(conn);
}

/* FUNÇOES DE CONTACTO COM A BASE-DE-DADOS */

void help (int socketfd)
{
	ostringstream
		oss;

	oss << "Bem-vindo ao Quem Quer Ser Milionário" << endl
	<< "Pode usar umas das seguintes funções:" << endl
	<< "-------------------------------------" << endl
	<< "\\help ..............................." << endl
	<< "Lista os comandos todos disponíveis  ;" << endl;

	string
		data = oss.str();
	writeline (socketfd, data);
}

void register (int socketfd, string &line)
{
	string
		comando,
		user,
		pass,
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
		PGresult* res = executeSQL ("SELECT * FROM utilizadores WHERE nome = '" + user + "'");
		
		if (PQntuples (res) > 0) // caso já existe um utilizador com o mesmo username 
		{
			writeline (socketfd, "Já existe um utilizador com este nome!");
		}
		else // caso não exista este username
		{
			executeSQL ("INSERT INTO utilizadores VALUES ('" + user +"', '" + pass + "', 'false', 'false')"); //não é admin nem está online
			writeline (socketfd, "Utilizador " + user + "criado!");
		}
	}
}

/* Trata de receber dados de um cliente cujo socketid foi passado como parâmetro */
void* cliente (void* args)
{
	int
		socketfd = *(int*) args;
	string
		line;

	clients.insert (socketfd);

	cout << "Client connected: " << socketfd << endl;
	while (readline (socketfd, line))
	{
		cout << "Socket " << socketfd << " said: " << line << endl;
		broadcast (socketfd, line);
		
		if (line.find ("\\help") == 0)
			help (socketfd);
		else if (line.find ("\\register") == 0)
			register (socketfd, *line);
	}

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
	listen(socketfd, 5);

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

	close (socketfd);
	return 0; 
}