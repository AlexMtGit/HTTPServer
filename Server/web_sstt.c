#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>


#define VERSION				24
#define BUFSIZE				8096
#define MAXLONGPATH			2048
#define ERROR				42
#define LOG					44
#define BADREQUEST			400
#define PROHIBIDO			403
#define NOENCONTRADO		404
#define NOACEPTABLE			406
#define MUYLARGA			414
#define NOIMPLEMENTADO		501
#define NOSOPORTADO 		505
#define REQUEST_TIMEOUT 	30
#define COOKIE_EXPIRATION 	10
#define MAX_COOKIES_AMOUNT	10


struct {
	char *ext;
	char *filetype;
} extensions [] = {
	{"gif", "image/gif" },//0
	{"jpg", "image/jpg" },//1
	{"jpeg","image/jpeg"},//2
	{"png", "image/png" },//3
	{"ico", "image/ico" },//4
	{"zip", "image/zip" },//5
	{"gz",  "image/gz"  },//6
	{"tar", "image/tar" },//7
	{"htm", "text/html" },//8
	{"html","text/html" },//9
	{0,0} };

struct {
	char *ans;
	char *msg;
} respuestas [] = {
	{"OK", "HTTP/1.1 200 OK\r\n" },					//0
	{"Server", "Server: www.sstt77853886.org\r\n" },//1
	{"ContentType", "Content-Type: " },				//2
	{"ContentLength", "Content-Length: " },			//3
	{"Date", "Date: " },							//4
	{"Connection", "Connection: Keep-Alive\r\n" },	//5
	{"KeepAlive", "Keep-Alive: timeout=" },			//6
	{"SetCookie", "Set-Cookie: " },					//7
	{"cookieCounter", "cookie_counter="},			//8
	{"maxAge", "; Max-Age="},						//9
	{0,0} };

char* errorResponses[]={
	"HTTP/1.1 ",							//0
	"403 Forbidden\r\n",					//1 403
	"404 Not Found\r\n",					//2 404
	"400 Bad Request\r\n",					//3 400
	"414 URI Too Long\r\n",					//4 414
	"501 Not Implemented\r\n",				//5 501
	"505 HTTP Version Not Supported\r\n\0",	//6 505
	"406 HTTP Version Not Supported\r\n",	//7 505
	0};

char* notImplementedMethods[]={
	"HEAD",		//0
	"POST",		//1
	"PUT",		//2
	"DELETE",	//3
	"CONNECT",	//4
	"OPTIONS",	//5
	"TRACE",	//6
	"PATCH",	//7
	0};

/**
	Método que añade a la respuesta del servidor la cabecera "ContentLength"
	Parámetros recibidos :
		response 				-	Cadena de respuesta en la que añadir la cabecera
		requestFileDescriptor	-	Descriptor del fichero al que calcular la longitud
**/
void addContentLengthHeader(char response[], int requestFileDescriptor)
{
	struct stat buf;
	fstat(requestFileDescriptor, &buf);
	int size = buf.st_size;
	char sizeString[10] = {0};
	sprintf(sizeString,"%d",size);

	strcat(response,respuestas[3].msg);
	strcat(response,sizeString);
	strcat(response,"\r\n");
	return;
}

/**
	Método que añade a la respuesta del servidor la cabecera "Date"
	Parámetros recibidos :
		response 				-	Cadena de respuesta en la que añadir la cabecera
**/
void addDateHeader(char response[])
{
	char date[BUFSIZE/8];
	time_t rawDate;
	struct tm *ts;
	time(&rawDate);
	ts = gmtime(&rawDate);
	char *weekDays[7] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
	char *months[12] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
	sprintf(date,"%s, %d %s %d %02d:%02d:%02d GMT\r\n" ,weekDays[ts->tm_wday],ts->tm_mday,months[ts->tm_mon],ts->tm_year + 1900,ts->tm_hour, ts->tm_min,ts->tm_sec);
	strcat(response,respuestas[4].msg);
	strcat(response,date);
	return;
}

/**
	Método que añade a la respuesta del servidor las cabeceras "Connection" y "KeepAlive"
	Parámetros recibidos :
		response 				-	Cadena de respuesta en la que añadir la cabecera
		timeout					-	Tiempo máximo que se mantendrá viva la conexión mientras no se reciban datos
**/
void addConnectionAndKeepAliveHeaders(char response[], int timeout)
{
	strcat(response,respuestas[5].msg);
	strcat(response,respuestas[6].msg);

	char timeoutString[5]= {0};
	sprintf(timeoutString,"%d",timeout);
	strcat(response,timeoutString);
	strcat(response,"\r\n");
	return;
}

/**
	Método que añade a la respuesta del servidor la cabecers "Set-Cookie"
	Parámetros recibidos :
		response 				-	Cadena de respuesta en la que añadir la cabecera
		cookieCounter			-	Valor del contador para cookie_counter
		maxAge					-	Tiempo máximo que se mantendrá viva la cookie
**/
void addSetCookieHeader(char response[], int cookieCounter, int maxAge)
{
	strcat(response,respuestas[7].msg);
	strcat(response,respuestas[8].msg);
	char n[5] = {0};
	sprintf(n,"%d",cookieCounter);
	strcat(response,n);
	strcat(response,respuestas[9].msg);
	sprintf(n,"%d",maxAge);
	strcat(response,n);
	strcat(response,"\r\n");
	return;
}

/**
	Método que calcula la longitud en caracteres de la respuesta
	Parámetros recibidos :
		response 				-	Cadena de respuesta a la que calcular el tamaño
	Valor de retorno :
		size					- 	Tamaño de la respuesta en caracteres
**/
int getResponseSize(char response[])
{
	int size = 0;
	while(response[size] != '\0')
	{
		size++;
	}
	return size;
}

void sendFileData(int socketFd, int sendFD)
{
	char readBuffer[BUFSIZE/2] = {0};
	int writeSize=0;
	while((writeSize = read(sendFD,readBuffer,BUFSIZE/2)) > 0)		//bucle de escritura del cuerpo del mensaje
	{
		write(socketFd,readBuffer,writeSize);
	}
	close(sendFD);
	return;
}

void createErrorResponse(int type,int socketFd, int cookieCounter)
{
	char response[BUFSIZE] = {0};
	strcat(response,errorResponses[0]);
	int htmlFD;
	switch (type)
	{
		case PROHIBIDO:
			strcat(response,errorResponses[1]);
			htmlFD = open("./403.html",O_RDONLY);
			break;
		case NOENCONTRADO:
			strcat(response,errorResponses[2]);
			htmlFD = open("./404.html",O_RDONLY);
			break;
		case BADREQUEST:
			strcat(response,errorResponses[3]);
			htmlFD = open("./400.html",O_RDONLY);
			break;
		case MUYLARGA:
			strcat(response,errorResponses[4]);
			htmlFD = open("./414.html",O_RDONLY);
			break;
		case NOIMPLEMENTADO:
			strcat(response,errorResponses[5]);
			htmlFD = open("./501.html",O_RDONLY);
			break;
		case NOSOPORTADO:
			strcat(response,errorResponses[6]);
			htmlFD = open("./505.html",O_RDONLY);
			break;
		case NOACEPTABLE:
			strcat(response,errorResponses[7]);
			htmlFD = open("./406.html",O_RDONLY);
			break;
	}
	//Server Header
	strcat(response,respuestas[1].msg);

	//Contentype Header
	strcat(response,respuestas[2].msg);
	strcat(response,extensions[8].filetype);
	strcat(response,"\r\n");

	//Añadimos el resto de cabeceras
	addContentLengthHeader(response,htmlFD);
	addDateHeader(response);
	addConnectionAndKeepAliveHeaders(response,REQUEST_TIMEOUT);
	addSetCookieHeader(response,cookieCounter,COOKIE_EXPIRATION);
	strcat(response,"\r\n");

	//Eviamos la respuesta HTTP
	write(socketFd,response,getResponseSize(response));

	//Enviamos los datos del fichero
	sendFileData(socketFd, htmlFD);
	return;
}

void debug(int log_message_type, char *message, char *additional_info, int socket_fd, int cookieCounter)
{
	int fd ;
	char logbuffer[BUFSIZE*2];

	switch (log_message_type)
	{
		case ERROR:
			(void)sprintf(logbuffer,"ERROR: %s:%s Errno=%d exiting pid=%d",message, additional_info, errno,getpid());
			break;
		case BADREQUEST:
			createErrorResponse(BADREQUEST,socket_fd, cookieCounter);
			(void)sprintf(logbuffer,"BAD REQUEST: %s:%s",message, additional_info);
			break;
		case PROHIBIDO:
			createErrorResponse(PROHIBIDO,socket_fd, cookieCounter);
			(void)sprintf(logbuffer,"FORBIDDEN: %s:%s",message, additional_info);
			break;
		case NOENCONTRADO:
			createErrorResponse(NOENCONTRADO,socket_fd, cookieCounter);
			(void)sprintf(logbuffer,"NOT FOUND: %s:%s",message, additional_info);
			break;
		case MUYLARGA:
			createErrorResponse(MUYLARGA,socket_fd, cookieCounter);
			(void)sprintf(logbuffer,"MUY LARGA: %s:%s",message, additional_info);
			break;
		case NOIMPLEMENTADO:
			createErrorResponse(NOIMPLEMENTADO,socket_fd, cookieCounter);
			(void)sprintf(logbuffer,"NOT IMPLEMENTED: %s:%s",message, additional_info);
			break;
		case NOSOPORTADO:
			createErrorResponse(NOSOPORTADO,socket_fd, cookieCounter);
			(void)sprintf(logbuffer,"NOT SUPPORTED: %s:%s",message, additional_info);
			break;
		case NOACEPTABLE:
			createErrorResponse(NOACEPTABLE,socket_fd, cookieCounter);
			(void)sprintf(logbuffer,"NOT SUPPORTED: %s:%s",message, additional_info);
			break;
		case LOG:
			(void)sprintf(logbuffer," INFO: %s:%s:%d",message, additional_info, socket_fd);
		 	break;
	}

	if((fd = open("webserver.log", O_CREAT| O_WRONLY | O_APPEND,0644)) >= 0)
	{
		(void)write(fd,logbuffer,strlen(logbuffer));
		(void)write(fd,"\n",1);
		(void)close(fd);
	}
	if(log_message_type == ERROR)
	{
		exit(3);
	}
}

/**
	Método que extrae el nombre del fichero solicitado en la petición
	y lo guarda en el segundo parámetro (path).
	Valores de retorno :
		i (tamaño de la ruta)	- Todo ha ido bien y la ruta está guardada en path
			--necesario para avanzar el puntero**
		-1						- La ruta del fichero es demasiada larga
		-2						- Error de sintaxis
**/
int getRequestPath(char* requestptr, char path[])
{

	//Comprobacion de sintaxis
	if (*requestptr!=' ')
	{
		return -2;
	}

	requestptr++;
	//Extraemos la ruta
	int i = 0;
	while (*requestptr!= ' ' && i<MAXLONGPATH && *requestptr!= '|')
	{
		path[i]=*requestptr;
		requestptr++; i++;
	}
	path[i]='\0';

	//Comprobacion de sintaxis
	if (path[0]=='\0')
	{
		return -2;
	}
	else if (i >= MAXLONGPATH)
	{
		return -1;
	}
	return i;
}

/**
	Método que comprueba la versión de protocolo que se usa en la petición
	Ademas, comprueba que la sintaxis de ese campo sea correcta
	Valores de retorno :
		0- Todo ha ido bien. La version es 1.0 o 1.1 y la sintaxis es correcta
		1- Sintaxis correcta, pero versión no soportada
		2- Sintaxis incorrecta
**/
int checkProtocolVersion(char* requestptr)
{
	if (*requestptr==' ')
	{
		requestptr++;
	}
	if (*requestptr=!'H' || *(requestptr +1)!='T'
		|| *(requestptr +2)!='T'|| *(requestptr +3)!='P' || *(requestptr +4)!='/'){
			return 2;
	}
	requestptr +=5;

	//Nos aseguramos de que es la version 1.0 o 1.1 de HTTP
	if (*requestptr == '1' && *(requestptr+1) == '.' && (*(requestptr+2) == '0' || *(requestptr+2) == '1')) {
			return 0;
	}

	return 1;

}

/**
	Método que comprueba si la cabecera "Host" está presente en la petición
	Valores de retorno :
		0- Si está contenida.
		1- No está contenida.
**/
int checkHostHeaderPresent(char* requestptr)
{
	if (strstr(requestptr,"Host: "))
	{
		return 0;
	}
	else
	{
		return 1;
	}
}

/**
	Método que comprueba la ubicación del fichero solicitado
	Con el fin de evitar accesos ilegales
	Valores de retorno :
		 0	- Todo ha ido bien. El fichero existe y se encuentra dentro de la jerarquía del servidor
		-1	- El fichero existe, pero se encuentra en un nivel superior de la jerarquía y, por tanto, se deniega el acceso
		-2	- El fichero no existe
**/
int checkFileAccess(char path[])
{
	char relativePath[MAXLONGPATH+2] = {0};
	strcat(relativePath, ".");
	strcat(relativePath,path);

	int fd = open(relativePath, O_RDONLY);
	//Dentro de la jerarquia
	if(fd != -1)
	{
		return fd;
	}
	else
	{
		fd = open(path, O_RDONLY);
		//Fuera de la jerarquía
		if(fd != -1)
		{
			return -1;
		}
		//No existe
		else{
			return -2;
		}
	}
}

/**
	Método que comprueba si el tipo de método de la petición (distinto de GET)
	es un método que existe en HTTP o no
	Valores de retorno :
		0- El método existe.
		1- El método no existe
**/
int checkIfMethodExists(char method[])
{
	for (int i=0; i< 8;i++)
	{
		if (strstr(method,notImplementedMethods[i]))
		{
			return 0;
		}
	}
	return 1;
}

/**
	Método que comprueba la extensión del fichero solicitado
	Con el fin de tratar solo determinadas extensiones
	Valores de retorno :
		NULL -	La extension no es soportada por el servidor
		!NULL-	La extensión se soporta y devolvemos el campo fileType correspondiente a la extension
**/
char* checkFileExtension(char path[])
{
	char *contentType = NULL;
	for (int i = 0; i<10; i++)
	{
		if (strstr(path,extensions[i].ext))
		{
			contentType = extensions[i].filetype;
		}
	}
	return contentType;
}

/**
	Método que añade a la respuesta del servidor toda la parte correspondiente a las cabeceras para una respuesta satisfactoria(CODIGO 200 OK)
	Parámetros recibidos :
		response 				-	Cadena de respuesta en la que añadir la cabecera
		contentType				-	Tipo de contenido del fichero que se enviará
		requestFileDescriptor	-	Descriptor del fichero que se enviará
		cookieCounter			-	Valor del contador para cookie_counter
**/
void addOkHeader(char response[], char* contentType, int requestFileDescriptor, int cookieCounter)
{
	strcat(response,respuestas[0].msg);
	strcat(response,respuestas[1].msg);
	strcat(response,respuestas[2].msg);
	strcat(response,contentType);
	strcat(response,"\r\n");

	addContentLengthHeader(response,requestFileDescriptor);
	addDateHeader(response);
	addConnectionAndKeepAliveHeaders(response,REQUEST_TIMEOUT);
	addSetCookieHeader(response,cookieCounter,COOKIE_EXPIRATION);
	strcat(response,"\r\n");
	return;
}


void process_web_request(int descriptorFichero, int cookieCounter)
{
	debug(LOG,"request","Ha llegado una peticion",descriptorFichero,cookieCounter);
	//
	// Definir buffer y variables necesarias para leer las peticiones
	//
    char buffer[BUFSIZE]= {0};
    char *bufferptr;
    int readsize = 0;
    char respuesta[BUFSIZE] = {0};
    char path[MAXLONGPATH+1] = {0};
    char *contentType;

	//
	// Leer la petición HTTP
	//
    readsize = read(descriptorFichero, buffer,  BUFSIZE-1);

	//
	// Comprobación de errores de lectura
	//
    if (readsize < 0)
    {
        debug(ERROR, "read", "Error en la lecturade peticion", descriptorFichero,cookieCounter);
    }
    if (readsize == 0)
    {
    	debug(BADREQUEST,"READ","No se han leido datos, cerrando conexion...",descriptorFichero,cookieCounter);
		close(descriptorFichero);
		exit(0);

    }

	//
	// Si la lectura tiene datos válidos terminar el buffer con un \0
	//
    buffer[readsize]='\0';

    if(cookieCounter > MAX_COOKIES_AMOUNT)
	{
		debug(PROHIBIDO,"MAX_COOKIE_REACHED","",descriptorFichero,cookieCounter);
		return;
	}

	//
	// Se eliminan los caracteres de retorno de carro y nueva linea
	//
    for (int i = 0; i<readsize;i++)
    {
        if (buffer[i]=='\r' || buffer[i]=='\n')
        {
            buffer[i]='|';
        }
    }
    bufferptr=buffer;




	//
	//	TRATAR LOS CASOS DE LOS DIFERENTES METODOS QUE SE USAN
	//	(Se soporta solo GET)
	//
	if (*bufferptr=='G' && *(bufferptr +1)=='E' && *(bufferptr +2)=='T'){
		//Avanzamos el puntero a la cadena que contiene la peticion
		bufferptr += 3;

		//
		//Extraemos la ruta del fichero solicitado
		//
		int pathChecked = getRequestPath(bufferptr,path);
		if (pathChecked == -1)
		{
			debug(MUYLARGA,"TOO LONG","La ruta del fichero es demasiado larga",descriptorFichero,cookieCounter);
			return;
		}
		if (pathChecked == -2)
		{
			debug(BADREQUEST,"BAD REQUEST","Error de sintaxis en la ruta del fichero",descriptorFichero,cookieCounter);
			return;
		}
		bufferptr +=pathChecked;

		//Comprobacion de sintaxis
		if (!*bufferptr==' '){
			debug(BADREQUEST,"BAD REQUEST","Falta espacio después de fichero",descriptorFichero,cookieCounter);
			return;
		}
		bufferptr +=1;

		//
		//Comprobamos la versión del protocolo
		//
		int versionSupported = checkProtocolVersion(bufferptr);
		if (versionSupported == 1)
		{
			debug(NOSOPORTADO,"NO SOPORTADO","Versión de protocolo no soportada",descriptorFichero,cookieCounter);
			return;
		}
		else if (versionSupported == 2)
		{
			debug(BADREQUEST,"BAD REQUEST","Error de sintaxis al especificar la versión del protocolo",descriptorFichero,cookieCounter);
			return;
		}
		bufferptr +=10;


		//
		//Comprobamos si contiene la cabecera HOST
		//
		int hostIncluded = checkHostHeaderPresent(bufferptr);
		if (hostIncluded==0)
		{
			debug(LOG,"Host","La cabecera Host está incluida en la petición",descriptorFichero,cookieCounter);
		}
		else
		{
			debug(LOG,"Host","La cabecera Host NO está incluida en la petición",descriptorFichero,cookieCounter);
		}

	}
	else
	{
		//El método de HTTP cuya longitud es más larga es CONNECT
		//La petición puede contener ese método, por tanto, reservamos
		//8 caracteres para el método.
		char method[8];
		for (int i = 0; i<8 && buffer[i]!= ' '; i++)
		{
			method[i]=buffer[i];
		}
		int exists = checkIfMethodExists(method);
		if (exists == 1)
		{
			debug(BADREQUEST,"BAD REQUEST","El método no existe",descriptorFichero,cookieCounter);
			return;
		}
		else
		{
			debug(NOIMPLEMENTADO,"NO IMPLEMENTADO","El método no está implementado",descriptorFichero,cookieCounter);
			return;
		}
	}

	//
	//	Como se trata el caso excepcional de la URL que no apunta a ningún fichero
	//	html
	//
	if (path[0] =='/' && path[1]=='\0')
	{
		path[1] = 'i'; path[2] = 'n'; path[3] = 'd'; path[4] = 'e'; path[5] = 'x';
		path[6] = '.'; path[7] = 'h'; path[8] = 't'; path[9] = 'm'; path[10] = 'l';
		path[11] = '\0';
	}

	//
	//	Como se trata el caso de acceso ilegal a directorios superiores de la
	//	jerarquia de directorios
	//	del sistema
	//
	int fd = checkFileAccess(path);
	if (fd == -1)
	{
		debug(PROHIBIDO,"PROHIBIDO","La ruta del fichero pedido es ilegal",descriptorFichero,cookieCounter);
		return;
	}
	else if (fd == -2)
	{
		debug(NOENCONTRADO,"No se ha encontrado el fichero",path,descriptorFichero,cookieCounter);
		return;
	}


	//
	//	Evaluar el tipo de fichero que se está solicitando, y actuar en
	//	consecuencia devolviendolo si se soporta u devolviendo el error correspondiente en otro caso
	//
	contentType = checkFileExtension(path);
	if (contentType == NULL)
	{
		debug(NOACEPTABLE,"NOACEPTABLE","La extensión del fichero no está soportada",descriptorFichero,cookieCounter);
		return;
	}

	//
	//	En caso de que el fichero sea soportado, exista, etc. se envia el fichero con la cabecera
	//	correspondiente, y el envio del fichero se hace en blockes de un máximo de  8kB
	//
	addOkHeader(respuesta, contentType,fd,cookieCounter);

	//Eviamos la respuesta HTTP
	write(descriptorFichero,respuesta,getResponseSize(respuesta));

	//Enviamos los datos del fichero
	sendFileData(descriptorFichero, fd);

	return;
}





int main(int argc, char **argv)
{
	int i, port, pid, listenfd, socketfd;
	socklen_t length;
	static struct sockaddr_in cli_addr;		// static = Inicializado con ceros
	static struct sockaddr_in serv_addr;	// static = Inicializado con ceros

	//  Argumentos que se esperan:
	//
	//	argv[1]
	//	En el primer argumento del programa se espera el puerto en el que el servidor escuchara
	if (argv[1] == NULL)
    {
        (void)printf("ERROR: Introduzca el puerto en el que escuchará el servidor\n");
        exit(4);
	}
    port = atoi(argv[1]);

	if(port < 0 || port >60000)
    {
		(void)printf("ERROR: Introduzca un número de puerto entre 1 y 60000\n");
        exit(4);
    }

	//
	//  argv[2]
	//  En el segundo argumento del programa se espera el directorio en el que se encuentran los ficheros del servidor
	//
    if (argv[2] == NULL)
    {
        (void)printf("ERROR: Introduzca el directorio en el que se ejecutará el servidor\n");
        exit(4);
    }

    if(chdir(argv[2]) == -1)
    {
        (void)printf("ERROR: No se puede cambiar de directorio %s\n",argv[2]);
        exit(4);
	}

	// Hacemos que el proceso sea un demonio sin hijos zombies
	if(fork() != 0)
		return 0; // El proceso padre devuelve un OK al shell


	(void)signal(SIGCHLD, SIG_IGN); // Ignoramos a los hijos
	(void)signal(SIGHUP, SIG_IGN); // Ignoramos cuelgues

	debug(LOG,"web server starting...", argv[1] ,getpid(),-1);

	/* setup the network socket */
	if((listenfd = socket(AF_INET, SOCK_STREAM,0)) <0)
		debug(ERROR, "system call","socket",0,-1);


	/*Se crea una estructura para la información IP y puerto donde escucha el servidor*/
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY); /*Escucha en cualquier IP disponible*/
	serv_addr.sin_port = htons(port); /*... en el puerto port especificado como parámetro*/

	if(bind(listenfd, (struct sockaddr *)&serv_addr,sizeof(serv_addr)) <0)
		debug(ERROR,"system call","bind",0,-1);

	if( listen(listenfd,64) <0)
		debug(ERROR,"system call","listen",0,-1);

	while(1){
		length = sizeof(cli_addr);
		if((socketfd = accept(listenfd, (struct sockaddr *)&cli_addr, &length)) < 0)
			debug(ERROR,"system call","accept",0,-1);
		if((pid = fork()) < 0) {
			debug(ERROR,"system call","fork",0,-1);
		}
		else {
			// Proceso hijo (uno por cada cliente conectado)
			if(pid == 0) {
				(void)close(listenfd);

				//Contador con el valor de cookie_counter
				int cookieCounter = 1;

				//Necesario para medir el tiempo sin recibir datos
				struct timeval tv;
				tv.tv_sec = REQUEST_TIMEOUT;
				tv.tv_usec = 0;
				int retval = 1;
				time_t beginning;
				time_t end;
 				time ( &beginning );

				//Procesamos la primera lllamada
				process_web_request(socketfd,cookieCounter); // El hijo termina tras llamar a esta función

				//Mientras tengamos datos y no salte el timeout
				while(retval > 0){
					//Reseteamos el conjunto de ficheros del select y el timeout
					fd_set rfds;
					FD_ZERO(&rfds);
					FD_SET(socketfd, &rfds);
					tv.tv_sec = REQUEST_TIMEOUT;
					tv.tv_usec = 0;
					retval = select(socketfd+1, &rfds, NULL, NULL, &tv);

					//Comprobamos la expiración de las cookeis
					time (&end);
					double diftimer = difftime(end,beginning);
					if(diftimer >= COOKIE_EXPIRATION)
					{
						cookieCounter = 0;
						time(&beginning);
					}

					//Si hay datos disponibles, procesamos la peticion
					if (retval){
						if(FD_ISSET(socketfd, &rfds)){
							cookieCounter++;
							process_web_request(socketfd,cookieCounter);
						}
					}
					//Si no tenemos datos y ha saldo el timeout cerramos la conexion y termina el proceso dedicado a dicha conexion
					else{
						close(socketfd);
						exit(0);
					}
				}
			}
			else { 	// Proceso padre
				(void)close(socketfd);
			}
		}
	}
}
