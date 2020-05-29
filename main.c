#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <microhttpd.h>
#include <string.h>

#define PORT 8888

char buffer[2048];

int get_measurements(char* buffer, size_t buffer_size)
{
  return snprintf(buffer, buffer_size, "Measurement=9001degrees\n WOW!\n");
}

int server_error(struct MHD_Connection *connection)
{
  printf("Server error response\n");
  const char *errorstr ="<html><body>An internal server error has occured!</body></html>";

  struct MHD_Response* response = MHD_create_response_from_buffer(strlen (errorstr),
                                                                  (void *) errorstr,
                                                                  MHD_RESPMEM_PERSISTENT);

  if (response){
    int ret = MHD_queue_response(connection, MHD_HTTP_INTERNAL_SERVER_ERROR, response);

    MHD_destroy_response(response);
    return ret;
  } else {
    return MHD_NO;
  }
}

int answer_to_connection (void *cls, 
                          struct MHD_Connection *connection,
                          const char *url,
                          const char *method, 
                          const char *version,
                          const char *upload_data,
                          size_t *upload_data_size, 
                          void **con_cls)
{
  if (0 != strcmp (method, "GET")) return MHD_NO;

  printf("New %s request for %s using version %s\n", method, url, version);

  int measurement_size = get_measurements(buffer, sizeof(buffer)-1);

  if(measurement_size < 0) {
    return server_error(connection); 
  }

  struct MHD_Response* response = MHD_create_response_from_buffer(measurement_size,
                                                                  (void*) buffer, 
                                                                  MHD_RESPMEM_MUST_COPY);

  printf("Sent measurement response\n");

  int ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
  
  MHD_destroy_response(response);

  return ret;
}

int main(int argc, char* argv[]) {
  struct MHD_Daemon *daemon;

  daemon = MHD_start_daemon(MHD_USE_INTERNAL_POLLING_THREAD, 
                            PORT, 
                            NULL, 
                            NULL, 
                            &answer_to_connection, 
                            NULL, 
                            MHD_OPTION_END);
  
  if (NULL == daemon) return 1;

  printf("Running websever on port: %d\n", PORT);
  getchar();

  MHD_stop_daemon(daemon); 
 
  return 0;
}
