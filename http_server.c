/**
* @Project	[HTTP Module]
* @File 	[http_server.c]
* @Author 	[caydyn@icloud.com]
* @Version 	[1.0]
*/

#include "http_server.h"

inline int check_authentication(struct MHD_Connection *);
inline void set_auth_cookie(struct MHD_Connection *);

/**
 * register request api handle
 * @param CallbackID [request handle id]
 * @param Func       [request handle func]
 */
int register_api_handle(uint32_t CallbackID,
                        APIFunc Func)
{
	if (CallbackID > MAX_API || CallbackID < 1)
		return (-1);

	REGISTER_API(CallbackID, Func);
	return 0;
}

/**
 * set response mime type
 * @param response [response handle]
 * @param type     [file type]
 */
inline void set_mime(struct MHD_Response *response,
                     uint8_t type)
{
	MHD_add_response_header(response,
	                        MHD_HTTP_HEADER_CONTENT_TYPE, mime_type_string[type]);
}

/**
 * response 200 ok
 * @param connection [http connect handle]
 */
inline void return_200 (struct MHD_Connection * connection)
{
	struct MHD_Response *response =
	    MHD_create_response_from_buffer (strlen (_200_page),
	                                     (void *) _200_page, MHD_RESPMEM_PERSISTENT);
	set_mime(response, HTM_FILE);
	MHD_queue_response (connection, MHD_HTTP_OK, response);
	MHD_destroy_response (response);
}

/**
 * response custom 200 ok
 * @param connection [http connect handle]
 * @param response_data [custom response data]
 * @param response_data [custom response data]
 */
inline void return_custom_200 (struct MHD_Connection * connection,
                               char * response_data, int mime_type)
{
	struct MHD_Response *response =
	    MHD_create_response_from_buffer (strlen (response_data),
	                                     (void *) response_data, MHD_RESPMEM_MUST_FREE);
	set_mime(response, mime_type);
	MHD_queue_response (connection, MHD_HTTP_OK, response);
	MHD_destroy_response (response);
}

/**
 * response 500 error
 * @param connection [http connect handle]
 */
inline void return_500 (struct MHD_Connection * connection, int err)
{
	char *string_of_error = malloc(1024);
	if (string_of_error)
		sprintf(string_of_error, _500_page, err);
	struct MHD_Response *response =
	    MHD_create_response_from_buffer (strlen (string_of_error),
	                                     (void *) string_of_error, MHD_RESPMEM_MUST_FREE);
	set_mime(response, HTM_FILE);
	MHD_queue_response (connection, MHD_HTTP_INTERNAL_SERVER_ERROR, response);
	MHD_destroy_response (response);
}

/**
 * response 404 error
 * @param connection [http connect handle]
 */
inline void return_404 (struct MHD_Connection * connection)
{
	struct MHD_Response *response =
	    MHD_create_response_from_buffer (strlen (_404_page),
	                                     (void *) _404_page, MHD_RESPMEM_PERSISTENT);
	set_mime(response, HTM_FILE);
	MHD_queue_response (connection, MHD_HTTP_NOT_FOUND, response);
	MHD_destroy_response (response);
}

/**
 * response 302
 * @param connection [http connect handle]
 */
inline void return_302 (struct MHD_Connection * connection, const char *url)
{
	struct MHD_Response *response =
	    MHD_create_response_from_buffer (strlen (_302_page),
	                                     (void *) _302_page, MHD_RESPMEM_PERSISTENT);
	MHD_add_response_header(response,
	                        MHD_HTTP_HEADER_LOCATION, url);
	MHD_queue_response (connection, MHD_HTTP_FOUND, response);
	MHD_destroy_response (response);
}

/**
 * get file information
 * @param  file_fd  [return file fd]
 * @param  file_path   [file path]
 * @param  file_size [return file size]
 * @return      [success: true, failure: <0]
 */
inline int
get_file_information (int *file_fd, char *file_path, int *file_size)
{
	struct stat file_stat_buf;

	if (!file_fd || !file_path || !file_size)
		return (-1);

	if ((*file_fd = open (file_path, O_RDONLY)) < 0)
		return (-2);

	if (fstat (*file_fd, &file_stat_buf) < 0)
		return (-3);

	if ((*file_size = file_stat_buf.st_size) < 0)
		return (-4);

	return 0;
}

int register_custom_file_path(const char *file_exten, const char *file_path)
{
	int len_of_path = strlen(file_exten);
	int len_of_path = strlen(file_path);

	uint8_t file_exten_len =
	    len_of_path > FILE_EXTEN_LEN_MAX ? FILE_EXTEN_LEN_MAX : len_of_path;
	uint8_t file_path_len =
	    len_of_path > FILE_PATH_LEN_MAX ? FILE_PATH_LEN_MAX : len_of_path;

	memcpy(CUSTOM_FILE_EXTEN[custom_file_path_nb], file_exten, file_exten_len);
	memcpy(CUSTOM_FILE_PATH[custom_file_path_nb], file_path, file_path_len);

	custom_file_path_nb++;
}

inline char *return_file_path(const char *file_exten)
{
	uint8_t i;

	for (i = 0; i < custom_file_path_nb; i++)
		if (strstr(CUSTOM_FILE_EXTEN[i], file_exten))
			return CUSTOM_FILE_PATH[i];

	return WEB_PATH;
}

/**
 * http request file
 * @param  url  [request url string]
 * @param  fd   [return file fd]
 * @param  size [return file size]
 * @param  type [return file type]
 * @return      [success: true, failure: false]
 */
inline static uint8_t url_to_file(const char *url, int *fd,
                                  int *size, int *type)
{
	struct stat f_stat;
	char file_full_path[512] = { 0 };
	char *file_path = NULL;

	if (strstr(url, ".html"))
	{
		*type = HTM_FILE;
		file_path = return_file_path("html");
	}
	else if (strstr(url, ".css"))
	{
		*type = CSS_FILE;
		file_path = return_file_path("css");
	}
	else if (strstr(url, ".js"))
	{
		*type = JAS_FILE;
		file_path = return_file_path("js");
	}
	else if (strstr(url, ".png"))
	{
		*type = PNG_FILE;
		file_path = return_file_path("png");
	}
	else if (strstr(url, ".jpg") || strstr(url, ".jpeg"))
	{
		*type = JPG_FILE;
		file_path = return_file_path("jpg");
	}
	else if (strstr(url, ".xml"))
	{
		*type = XML_FILE;
		file_path = return_file_path("xml");
	}
	else if (strstr(url, ".csv"))
	{
		*type = CSV_FILE;
		file_path = return_file_path("csv");
	}
	else if (strstr(url, ".pdf"))
	{
		*type = PDF_FILE;
		file_path = return_file_path("pdf");
	}
	else if (strstr(url, ".exe"))
	{
		*type = APP_FILE;
		file_path = return_file_path("exe");
	}
	else if (strstr(url, ".json") || strstr(url, ".map"))
	{
		*type = JSO_FILE;
		file_path = return_file_path("json");
	}
	else if (strstr(url, ".txt") || strstr(url, ".conf"))
	{
		*type = TXT_FILE;
		file_path = return_file_path("txt");
	}
	else if (strstr(url, ".woff") || strstr(url, ".woff2"))
	{
		*type = WOF_FILE;
		file_path = return_file_path("woff");
	}
	else if (strstr(url, ".ttf"))
	{
		*type = TTF_FILE;
		file_path = return_file_path("ttf");
	}
	else if (strstr(url, ".eot"))
	{
		*type = EOT_FILE;
		file_path = return_file_path("eot");
	}
	else if (strstr(url, ".otf"))
	{
		*type = OTF_FILE;
		file_path = return_file_path("otf");
	}
	else if (strstr(url, ".svg"))
	{
		*type = SVG_FILE;
		file_path = return_file_path("svg");
	}
	else if (strstr(url, ".mp3"))
	{
		*type = MP3_FILE;
		file_path = return_file_path("mp3");
	}
	else if (strstr(url, ".wav"))
	{
		*type = WAV_FILE;
		file_path = return_file_path("wav");
	}
	else if (strstr(url, ".gif"))
	{
		*type = GIF_FILE;
		file_path = return_file_path("gif");
	}
	else if (strstr(url, ".mp4"))
	{
		*type = MP4_FILE;
		file_path = return_file_path("mp4");
	}
	else if (strstr(url, ".avi"))
	{
		*type = AVI_FILE;
		file_path = return_file_path("avi");
	}
	else if (strstr(url, ".flv"))
	{
		*type = FLV_FILE;
		file_path = return_file_path("avi");
	}
	else
	{
		*type = HTM_FILE;
		file_path = WEB_PATH;
	}

	if (!strcmp(url, "/"))
		sprintf(file_full_path, "%s/%s", return_file_path, "index.html");
	else
		sprintf(file_full_path, "%s%s", return_file_path, url);

	if (get_file_information (fd, file_full_path, size) < 0)
		return false;

	return true;
}

/**
 * http request api
 * @param  url        [request url string]
 * @param  connection [http connet handle]
 * @param  data       [post data]
 * @param  size       [post data size]
 * @return            [success: true, failure: false]
 */
inline uint8_t url_to_api(const char *url,
                          struct MHD_Connection * connection, uint8_t upload_file,
                          char *data, uint32_t size, const char *file_name)
{
	int err, api_id = atoi(url + 1);	/* remove '/' */
	char *response_data = NULL;

	uint8_t enable_redirect = false;
	char redirect_url[2048] = { 0 };

	/*
		API ID 0, just reserve for login
	*/
	if (api_id == 0)
	{
		if (!strcmp(data, LOGIN_AUTHENTICATE_PASSWD))	//check login ok
		{
			set_auth_cookie(connection);
			return true;
		}
		else
			return_404(connection);
	}

	/*
		process all register api
	 */
	if (APIFA[api_id])
	{
		_cur_connect = connection;
		err = APIFA[api_id]((const char*)data, (const uint32_t)size,
		                    &response_data, &enable_redirect, redirect_url);
	}
	else
		return_404(connection);

	_cur_connect = NULL;

	if (err < 0)
	{
		return_500(connection, err);
		return true;
	}
	else
	{
		if (response_data)
			return_custom_200(connection, response_data, JSO_FILE);
		else
		{
			if (enable_redirect)
				return_302(connection, redirect_url);
			else
				return_200(connection);
		}
		return false;
	}
}

/**
 * get request api url param
 * @param  key [url param key]
 * @return     [success: value, failure: NULL]
 */
inline const char *get_url_param(char *key)
{
	if (_cur_connect)
		return MHD_lookup_connection_value(_cur_connect,
		                                   MHD_GET_ARGUMENT_KIND, key);
	else
		return NULL;
}

/**
 * process http get request
 * @param url        [request url string]
 * @param connection [http connect handle]
 */
inline void process_get_url_requert(const char *url,
                                    struct MHD_Connection * connection)
{
	struct MHD_Response *response;
	int fd, size, type;

	if (url_to_file(url, &fd, &size, &type))
	{
		response =
		    MHD_create_response_from_fd(size, fd);
		set_mime(response, type);
		MHD_queue_response(connection, MHD_HTTP_OK, response);
		MHD_destroy_response(response);
		return;
	}
	else if (url_to_api(url, connection, false, NULL, 0, NULL))
		return;
	else
		return_404(connection);
}

/**
 * requset post file
 * @param  content   [post data]
 * @param  len       [current post data len]
 * @param  file_name [save file name]
 * @param  file_size [post file total size]
 * @return           [current success save file size]
 */
static inline int upload_handle(char *content,
                                int len, char *file_name, int *file_size)
{
	int filter = UPLOAD_SYMBLE_NB;
	char *real = NULL, *find_size = NULL, get_size[4] = { 0 };
	int fd, size, write_size = 0;

	if (!content)
		return (-1);

	find_size = strstr(content, FILE_SIZE_FLAG);
	if (find_size)
	{
		memcpy(get_size, find_size + SIZE_OFFSET + 1,
		       strchr(find_size + SIZE_OFFSET + 1, DOUBLE_QUOTES) -
		       strchr(find_size + SIZE_OFFSET, DOUBLE_QUOTES) - 1);
	}

	*file_size = atoi(get_size);

	real = strstr(content, UPLOAD_SYMBLE);
	real++;
	while (filter)
	{
		real = strstr(real, UPLOAD_SYMBLE);
		real++;
		filter--;
	}

	write_size = len - (real - content) - 1;
	if (write_size > *file_size)
		write_size = *file_size;

	fd = open(file_name, O_RDWR | O_CREAT);
	size = write(fd, real + 1, write_size);
	close(fd);

	return size;
}

/**
 * frist process upload file
 * @param info [last connect handle]
 * @param data [upload data]
 * @param size [upload data size]
 */
static inline void upload_Begin_proc(next_connection_info* info,
                                     char *data, int size)
{
	upload_handle(data, size, info->file_name, &(info->file_size));
}

static inline void upload_Other_proc(next_connection_info* info,
                                     char* data, int size)
{
	int err, fd, len;

	if (!info->after)
	{
		err = upload_handle(data, size, info->file_name,
		                    &(info->file_size));
		if (err > 0)
		{
			info->used += err;
			info->after = true;
		}
	}
	else
	{
		fd = open(info->file_name, O_RDWR | O_APPEND);
		if (info->used + size < info->file_size)
			len = size;
		else
			len = info->file_size - info->used;
		err = write(fd, data, len);
		info->used += err;
		close(fd);
	}
}

inline int check_authentication(struct MHD_Connection *connection)
{
	struct MHD_Response *response;
	const char *ver = NULL;
	int fd;
	int file_fd = 0, file_size = 0;
	uint64_t cookie_value = 0;

	//get request cookie by cookie name
	ver = MHD_lookup_connection_value (connection, MHD_COOKIE_KIND,
	                                   AUTHENTICATE_COOKIE_NAME);
	if (ver)
	{
		cookie_value = atoll(ver);
		if (cookie_value != last_cookie)
			goto __AUTH_FAILED;
		return 0;
	}
	else
	{
__AUTH_FAILED:
		get_file_information (&file_fd, LOGIN_HTML_PATH, &file_size);
		response = MHD_create_response_from_fd (file_size, file_fd);
		MHD_queue_response (connection, MHD_HTTP_OK, response);
		MHD_destroy_response (response);
		return (-1);
	}
}

static inline uint64_t get_random_cookie(void)
{
	uint64_t cookie;
	cookie = lrand48();
	cookie <<= 32;
	cookie += lrand48();
	return cookie;
}

inline void set_auth_cookie(struct MHD_Connection *connection)
{
	char response[64] = { 0 };
	char cookie[256] = { 0 };

	sprintf(response, "{\"login\": \"YES\"}");
	last_cookie = get_random_cookie();
	sprintf(cookie, AUTHENTICATE_COOKIE_FORAMT, AUTHENTICATE_COOKIE_NAME,
	        last_cookie, AUTHENTICATE_COOKIE_TIME);

	response = MHD_create_response_from_buffer (strlen(login),
	           (void *)(login), MHD_RESPMEM_PERSISTENT);
	MHD_add_response_header (response, MHD_HTTP_HEADER_SET_COOKIE,
	                         set_coockie);
	MHD_queue_response (connection, MHD_HTTP_OK, response);
	MHD_destroy_response (response);
}

/**
 * http request handle
 */
inline int http_request_handle (void *cls, struct MHD_Connection *connection, const char *url,
                                const char *method, const char *version, const char *upload_data,
                                size_t * upload_data_size, void **con_cls)
{
#if ENABLE_AUTHENTICATE
	//check request authenticate(cookie)
	if (!(strstr(url , AUTHENTICATE_LOGIN_URL)))
	{
		if (check_authentication(connection) < 0)
			return MHD_YES;
	}
#endif

	if (!(*con_cls))
	{
		//process GET request or first POST request

		if (!strcmp(method, MHD_HTTP_METHOD_POST))	//POST
		{
			next_connection_info *next_connection;
			next_connection = calloc(sizeof(char),
			                         sizeof(next_connection_info));
			strcpy(next_connection->url, url);
			strcpy(next_connection->method, MHD_HTTP_METHOD_POST);
			next_connection->post_data_len
			    = atoi (MHD_lookup_connection_value (connection,
			            MHD_HEADER_KIND,
			            MHD_HTTP_HEADER_CONTENT_LENGTH));

			if (strstr(url, "upload"))
			{
				const char *name = MHD_lookup_connection_value(connection,
				                   MHD_GET_ARGUMENT_KIND, "file_name");
				if (!name)
				{
					free(next_connection);
					return MHD_NO;
				}
				else
					strcpy(next_connection->file_name, name);


				next_connection->post_data = NULL;
			}
			else
			{
				next_connection->post_data = calloc(sizeof(char),
				                                    next_connection->post_data_len);
				if (!next_connection->post_data)
					return MHD_NO;
			}
			*con_cls = (void *)next_connection;
		}
		else if (!strcmp(method, MHD_HTTP_METHOD_GET))	//GET
			process_get_url_requert(url, connection);
	}
	else
	{
		next_connection_info *next_connection = ((next_connection_info *)(*con_cls));

		if (!strcmp(next_connection->method,  MHD_HTTP_METHOD_POST))
		{
			if (*upload_data_size)
			{
				if (next_connection->post_data_len == *upload_data_size)
				{
					if (strstr(next_connection->url, "upload"))
						upload_Begin_proc(next_connection,
						                  (char *)upload_data, *upload_data_size);
					else
						memcpy(next_connection->post_data, upload_data,
						       next_connection->post_data_len);

					*upload_data_size = 0;
				}
				else
				{
					if (strstr(next_connection->url, "upload"))
						upload_Other_proc(next_connection,
						                  (char *)upload_data, *upload_data_size);
					else
						memcpy((next_connection->post_data + next_connection->offset),
						       upload_data, *upload_data_size);

					next_connection->offset += *upload_data_size;
					*upload_data_size = 0;
				}
			}
			else
			{
				url_to_api(url, connection, true,
				           next_connection->post_data,
				           next_connection->post_data_len,
				           next_connection->file_name);

				if (next_connection->post_data)
				{
					free(next_connection->post_data);
					free(next_connection);
				}
			}
		}
		else
			return_404(connection);
	}

	return MHD_YES;
}

/**
 * init http server
 * @return [http server handle]
 */
uint8_t init_http_server(char *path)
{
	uint8_t i;

	if (!path)
		return false;

	if (strlen(path) > FILE_PATH_LEN_MAX)
		return false;

	sprintf(WEB_PATH, "%s", path);

	for (i = 0; i < MAX_FILE_CUSTOM_PATH; i++)
	{
		memset(CUSTOM_FILE_EXTEN[i], 0, FILE_EXTEN_LEN_MAX);
		memset(CUSTOM_FILE_PATH[i], 0, FILE_PATH_LEN_MAX);
	}
	custom_file_path_nb = 0;

	_http_daemon = MHD_start_daemon (
	                   MHD_USE_SELECT_INTERNALLY,
	                   PORT_NUMBER, NULL, NULL, &http_request_handle, NULL,
	                   MHD_OPTION_CONNECTION_TIMEOUT, (unsigned int) 120, MHD_OPTION_END);
	if (!_http_daemon)
		return false;

	srand48(time(NULL));

	printf ("HTTP Server is running on %d.\n", PORT_NUMBER);
	return true;
}

/**
 * clear http handle
 */
void release_http_server(void)
{
	MHD_stop_daemon(_http_daemon);
}