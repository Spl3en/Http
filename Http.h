/**
 *  @author     :   Spl3en
 *  @file       :   Http.h
 *  @version    :   1.1
 *
 *  Http est une librairie permettant de récupérer de façon transparente du contenu via http.
 *
 *  Changelog :
 *
 *		[+] 1.1
 *          [FIX]   Memory allocation in http_full_src
 *          [ADD]   http_header_and_src
 *          [ADD]   http_image
 *          [ADD]   http_div
 *
*/

#ifndef http_H_INCLUDED
#define http_H_INCLUDED

/* Librairies */
#include <windows.h>
#include <assert.h>
#include <fcntl.h>
#include "../Ztring/Ztring.h"
#include "../EasySocket/EasySocket.h"
#include "../Utils/Utils.h"

#define HTTP_BUFFER_SIZE        1024 * 100
#define HTTP_PORT               80
#define HTTP_MALFORMED_HEADER   ((void*)400)

typedef
struct _Http
{
    SOCKET s;
    char buffer[HTTP_BUFFER_SIZE];

}	Http;

typedef
struct _HttpAnswer
{
    char *header;
    char *src;

    int len_header;
    int len_src;

} HttpAnswer;

typedef
struct _HttpHeader
{
    char *method;
    char *path;
    char *file;

    char *post;

}   HttpHeader;

	/** * * * * * * * *
	*  @Constructors  *
	* * * * * * * * * */

HttpHeader *
http_header_new (char *msg);


	/** * * * * * * * *
	*     @Methods    *
	* * * * * * * * * */

HttpAnswer *
http_header_and_src (char *addr, char *data);

char *
http_filename_from_link (char *link);

char *
http_get (char *addr);

char *
http_post (char *addr, char *data);

int
http_image(char *addr, char *dest);

char *
http_div(char *str, char *div_line);

void
http_add_field(char *field);

void
http_answer(EasySocket *es, char *text);

void
http_set_keep_alive (int _keep_alive);

void
http_set_stop_condition (bool (*boolean_function) (char *buffer, int size));

void
http_close ();

	/** * * * * * * * *
	*   @Destructors  *
	* * * * * * * * * */

#endif
