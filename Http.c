/**
*	@author 	:	Spl3en
*	@file 		:	Http.c
*
*	Furthermore informations in Http.h
*/

#include "Http.h"
Http *http = NULL;
char *http_field = NULL;
bool wsa_init = false;
bool keep_alive = true;
bool (*stop_condition) (char *buffer, int size);

/**
*	Private Methods
*/

static HttpAnswer *
http_answer_new (char *header, int len_header, char *src, int len_src)
{
    HttpAnswer *answer = malloc (sizeof (HttpAnswer));
    if (answer == NULL)
        return NULL;

    answer->header = header;
    answer->len_header = len_header;
    answer->src = src;
    answer->len_src = len_src;

    return answer;
}

static void
http_divide (char *addr, char *host, char *path)
{
    if (strstr (addr, "http://"))
        addr = &addr[strlen ("http://")];

    int len = strlen (addr);
    int i;

    for (i = 0; i < len && addr[i] != '/'; i++) {
        host[i] = addr[i];
    }

    host[i] = '\0';
    strcpy (path, &addr[i]);

    len = strlen (path);

    for (i = 0; i < len; i++)
        if (path[i] == ' ')
            path[i] = '+';
}

static char *
http_ip_from_hostname (char *addr)
{
    struct hostent *h;
    char *host = NULL;
    char *path = NULL;

    str_cpy (&host, addr);
    str_cpy (&path, addr);

    http_divide (addr, host, path);

	if ((h = gethostbyname (host)) == NULL)
    {
        free (host);
        free (path);
        return NULL;
    }

    free (path);
    free (host);

	return inet_ntoa (* ((struct in_addr *)h->h_addr));
}

static SOCKET
http_socket (char *hostname)
{
	char *ptrNewHostname = NULL;
    int port = HTTP_PORT;

    if (strstr (hostname, "http://"))
        hostname = &hostname[strlen ("http://")];

	char *portStr;
    if ((portStr = str_pos_ptr (hostname, ":"))) {
		char *portStrAfter;
		char *portStrReplace;
		port = strtol (portStr+1, &portStrAfter, 10);
		strn_cpy_alloc (&portStrReplace, portStr, (portStrAfter - portStr));
		hostname = str_replace (portStrReplace, "", hostname);
		ptrNewHostname = hostname;
    }

    char *ip = http_ip_from_hostname (hostname);

    if (ptrNewHostname)
		free (ptrNewHostname);

    if (ip == NULL)
        return -1;

    SOCKET sock = 0;
    SOCKADDR_IN server_context;
    SOCKADDR_IN csin;
    int csin_size = sizeof (csin);

    sock = socket (AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET)
        printf ("Error %s : INVALID_SOCKET.\n", __FUNCTION__);

    server_context.sin_family      = AF_INET;
    server_context.sin_addr.s_addr = inet_addr (ip);
    server_context.sin_port        = htons (port);

    if (connect (sock, (SOCKADDR*)&server_context, csin_size) == SOCKET_ERROR)
        printf ("Error %s : SOCKET_ERROR.\n", __FUNCTION__);

	// Set non blocking
	bool blocking = false;
	#ifdef WIN32
		unsigned long on = (blocking) ? 0 : 1;
		if (0 != ioctlsocket (sock, FIONBIO, &on))
			return -1;
	#else
		int flags = fcntl (sock, F_GETFL, 0);
		if (flags < 0)
			return -1;
		flags = blocking ? (flags&~O_NONBLOCK) : (flags|O_NONBLOCK);
		if (fcntl (sock, F_SETFL, flags) != 0)
			return -1;
	#endif

    return sock;
}

static void
http_free()
{
	if (http == NULL)
		return;

	free (http);
	http = NULL;
}

static void
http_new (char *hostname)
{
	http_free();

	Http *p = calloc (sizeof (Http), 1);

	if (p == NULL)
		return;

    p->s = http_socket (hostname);
    if (p->s == (unsigned) -1)
        printf ("%s : Get ip from hostname failed.\n", __FUNCTION__);

    memset (p->buffer, '\0', sizeof (p->buffer));

    http = p;
}

void
http_close()
{
	if (http != NULL)
	{
		closesocket (http->s);
		http_free();
	}
}

void
http_init()
{
	if (!wsa_init)
	{
		WSADATA wsaData;
		if (WSAStartup (MAKEWORD (2, 0), &wsaData) != 0)
		{
			printf ("Erreur WSAStartUp.");
		}
		else
			wsa_init = true;
	}
}

/**
*	Public Methods
*/
HttpHeader *
http_header_new (char *msg)
{
    HttpHeader *hh = NULL;
    char buffer[1024];
    char local_method[32];
    unsigned int i;

    char *tmp = NULL;
    int pos;

    if ((hh = (HttpHeader *) malloc (sizeof (HttpHeader))) == NULL)
        return NULL;

    str_getline (msg, buffer, sizeof (buffer), 0);

    if ((pos = str_get_word (buffer, local_method, sizeof (local_method))) < 0)
        return HTTP_MALFORMED_HEADER;

    tmp = &buffer[pos + 1];

    if (str_get_word (tmp, buffer, sizeof (buffer)) < 0)
        return HTTP_MALFORMED_HEADER;

    for (i = strlen (buffer); i != 0; i--)
    {
        if (buffer[i] == '/')
            break;
    }

    if (i <= strlen (buffer))
    {
        str_cpy (&hh->file, &buffer[i+1]);
        buffer[i+1] = '\0';
    }
    else
        hh->file = "";

    str_cpy (&hh->path, buffer);
    str_cpy (&hh->method, local_method);

    return hh;
}

void
http_send_request (char *addr, char *post_data)
{
    char *host = NULL;
    char *path = NULL;
    char *str = NULL;

    str_cpy (&host, addr);
    str_cpy (&path, addr);

    http_divide (addr, host, path);

    str = str_dup_printf
    (
        "%s %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "User-Agent: Mozilla/5.0 (Windows NT 6.1; WOW64; rv:31.0) Gecko/20100101 Firefox/1337.0\r\n"
        "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n"
        "Accept-Language: fr,fr-fr;q=0.8,en-us;q=0.5,en;q=0.3\r\n"
        "Accept-Encoding: deflate\r\n"
        "%s"
        "Connection: %s\r\n"
        "%s",
        (!post_data) ? "GET" : "POST",
        path, host,
        (http_field) ? http_field : "",
		 (keep_alive) ? "keep-alive" : "closed",
		 (!post_data) ? "\r\n" : str_dup_printf (
			"Content-Type: application/x-www-form-urlencoded\r\n"
			"Content-Length: %d\r\n\r\n"
			"%s",
				strlen (post_data),
				post_data
		)
    );

    send (http->s, str, strlen (str), 0);

    free (path);
    free (host);
    free (str);
}

char *
http_recv_answer (int *answerLen)
{
    int bReceived, totalLen = 0;
	char *answer  = NULL;
	char *message = NULL;
    BbQueue messages_received = bb_queue_local_decl();

	do {
		memset (http->buffer, 0, sizeof (http->buffer));
		bReceived = recv (http->s, http->buffer, sizeof (http->buffer), 0);
		if (bReceived != 0 && bReceived != -1) {
			totalLen += bReceived;
			strn_cpy_alloc (&message, http->buffer, bReceived);
			bb_queue_add (&messages_received, message);
			if (stop_condition != NULL
			&&  stop_condition (http->buffer, bReceived)) {
				break;
			}
		}
		Sleep (1);
	} while (bReceived != 0);

	if (!keep_alive) {
		http_close();
	}

    answer = str_malloc_clear (totalLen + 1);
    while (bb_queue_get_length (&messages_received)) {
    	char *message = bb_queue_get_first (&messages_received);
		strcat (answer, message);
		free (message);
    }

    *answerLen = totalLen;

    return answer;
}

void
http_set_stop_condition (bool (*boolean_function) (char *buffer, int size))
{
	stop_condition = boolean_function;
}

char *
http_full_src (char *addr, int *answerLen, char *post_data)
/**
*   @return : The string containing the source code of the URL
*   @param  : char *addr      :   The URL
*   @param  : int *answerLen  :   allocated integer pointer which contains the length of the string returned
*	@param  : char *post_data :   Data that goes to the server (POST request only, NULL for GET method)
*/
{
	char *answer  = NULL;

    http_init();

    if (http == NULL || !keep_alive) {
    	http_new (addr);
    }

	http_send_request (addr, post_data);
	answer = http_recv_answer (answerLen);

    return answer;
}

HttpAnswer *
http_header_and_src (char *addr, char *data)
{
    int len;
    char *str = NULL;
    char end_of_header[] = "\r\n\r\n";
    int buflen = strlen (end_of_header) + 1;
    char *buffer = malloc (buflen);
    char *header, *src;
    int i;
    addr = strdup (addr);

    str = http_full_src (addr, &len, data);

    if (!str)
        return NULL;

    memset (buffer, '\0', buflen);

    for (i = 0; i < len; i++)
    {
        if (strcmp (buffer, end_of_header) == 0)
            break;

        memmove (buffer, (buffer + 1), buflen - 2);
        buffer[buflen-2] = str[i];
    }

    header = malloc (i + 1);
    str_substring (str, 0, i - 1, header);

    src = malloc (len - i + 1);
    if (i != len)
		str_substring (str, i, len, src);
	else
		src[0] = '\0';

	free (addr);
    return http_answer_new (header, i + 1, src, len - i + 1);
}

int
http_image (char *addr, char *dest)
{
    HttpAnswer *a = http_header_and_src (addr, NULL);
    return file_save_binary (dest, &a->src[6], a->len_src);
}

char *
http_filename_from_link (char *link)
{
    int len = strlen (link);
    int i;

    for (i = len; i >= 0; i--)
    {
        if (link[i] == '/')
            return &link[i+1];
    }

    return NULL;
}

char *
http_div (char *str, char *div_line)
{
    int div_pos = str_pos (str, div_line);
    int i, len = strlen (str);
    char buffer[6];
    int recursive = 0;
    char *ret = NULL;
    char *start = strdup ("<div ");
    char *end = strdup ("</div");

    memset (buffer, '\0', sizeof (buffer));

    for (i = div_pos + strlen (div_line); i < len; i++)
    {
        if (strstr (buffer, start))
            recursive++;

        if (strstr (buffer, end))
        {
            recursive--;

            if (recursive < 0)
            {
                ret = malloc (i - div_pos + 1);
                str_substring (str, div_pos, i, ret);
                break;
            }
        }

        memmove (buffer, buffer + 1, 4);
        buffer[4] = str[i];
    }

    free (start);
    free (end);

    return ret;
}


char *
http_get (char *addr)
{
    return http_post (addr, NULL);
}

char *
http_post (char *addr, char *data)
{
    char *src = NULL;
    HttpAnswer *answer = http_header_and_src (addr, data);

    if (!answer)
        return NULL;

    str_cpy (&src, answer->src);

    free (answer->header);
    free (answer->src);
    free (answer);

    return src;
}


void
http_answer (EasySocket *es, char *text)
{
    char *res =
        "HTTP/1.1 200 OK\r\n"
        "Date: Thu, 23 Jun 2011 20:13:47 GMT\r\n"
        "Server: Apache/2.2.16 (Unix) mod_ssl/2.2.16 OpenSSL/0.9.8o\r\n"
        "X-Powered-By: PHP/5.2.13-pl1-gentoo\r\n"
        "Expires: Thu, 21 Jul 1977 07:30:00 GMT\r\n"
        "Cache-Control: post-check=0, pre-check=0\r\n"
        "Pragma: no-cache\r\n"
        "Last-Modified: Thu, 23 Jun 2011 20:13:47 GMT\r\n"
        "Vary: Accept-Encoding,User-Agent\r\n"
        "Content-Length: 7370\r\n"
        "Keep-Alive: timeout=15, max=100\r\n"
        "Connection: Keep-Alive\r\n"
        "Content-Type: text/html\r\n\r\n";

    char *send_msg = str_dup_printf ("%s%s", res, text);

    es_send (es, send_msg, -1);

    free (send_msg);
}

void
http_add_field (char *field)
{
    if (http_field != NULL)
    {
        char *old = http_field;
        http_field = str_dup_printf ("%s%s\r\n", old, field);
        free (old);
    }

    else {
        http_field = str_dup_printf ("%s\r\n", field);
    }
}

void
http_set_keep_alive (bool _keep_alive)
{
	keep_alive = _keep_alive;
}
