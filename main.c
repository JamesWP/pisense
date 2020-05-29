#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <microhttpd.h>
#include <string.h>
#include <fcntl.h>

#include "bme280.h"

#define PORT 8888

#define METRIC_TEMP_NAME "bme280_temperature"
#define METRIC_TEMP_HELP "temperature in degrees celcius"
#define METRIC_TEMP_TYPE "gauge"

#define METRIC_PRES_NAME "bme280_pressure"
#define METRIC_PRES_HELP "pressure in hectopascals (hPa)"
#define METRIC_PRES_TYPE "gauge"

#define METRIC_HUMD_NAME "bme280_humidity"
#define METRIC_HUMD_HELP "percentage"
#define METRIC_HUMD_TYPE "gauge"

char buffer[8094];

int fd;
struct bme280_dev dev;
uint32_t req_delay;

int8_t user_i2c_read(uint8_t id, uint8_t reg_addr, uint8_t *data, uint16_t len);
void user_delay_ms(uint32_t period);
int8_t user_i2c_write(uint8_t id, uint8_t reg_addr, uint8_t *data, uint16_t len);

int write_metric(char*buffer, 
                 size_t buffer_size, 
                 int device_id, 
                 char* metric_name, 
                 char* metric_help, 
                 char* metric_type,
                 float value)
{
  int total = 0;

  total += snprintf(buffer, buffer_size, "# HELP %s %s\n", metric_name, metric_help);
  total += snprintf(buffer+total, buffer_size-total, "# TYPE %s %s\n", metric_name, metric_type);
  total += snprintf(buffer+total, buffer_size-total, "%s{device_address=\"0x%02x\"} %.2lf\n", metric_name, device_id, value);

  return total;
}

int get_measurements(char* buffer, size_t buffer_size)
{
  int total = 0;
  int device_id = dev.dev_id;

  /* Structure to get the pressure, temperature and humidity values */
  struct bme280_data comp_data;

  /* Set the sensor to forced mode */
  int8_t rslt = bme280_set_sensor_mode(BME280_FORCED_MODE, &dev);
  if (rslt != BME280_OK)
  {
      fprintf(stderr, "Failed to set sensor mode (code %+d).", rslt);
      return -1;
  }

  /* Wait for the measurement to complete and print data */
  dev.delay_ms(req_delay);
  rslt = bme280_get_sensor_data(BME280_ALL, &comp_data, &dev);
  if (rslt != BME280_OK)
  {
      fprintf(stderr, "Failed to get sensor data (code %+d).", rslt);
      return -1;
  }

  float temp = comp_data.temperature;
  float press = 0.01 * comp_data.pressure;
  float hum = comp_data.humidity;

  total += write_metric(buffer + total, 
                        buffer_size - total, 
                        device_id, 
                        METRIC_TEMP_NAME, 
                        METRIC_TEMP_HELP, 
                        METRIC_TEMP_TYPE, 
                        temp);

  total += write_metric(buffer + total, 
                        buffer_size - total, 
                        device_id, 
                        METRIC_PRES_NAME, 
                        METRIC_PRES_HELP, 
                        METRIC_PRES_TYPE, 
                        press);

  total += write_metric(buffer + total, 
                        buffer_size - total, 
                        device_id, 
                        METRIC_HUMD_NAME, 
                        METRIC_HUMD_HELP, 
                        METRIC_HUMD_TYPE, 
                        hum);

  return total;
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

int answer_to_connection(void *cls, 
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

  /* Variable to define the result */
  int8_t rslt = BME280_OK;

  if (argc < 2) {
    fprintf(stderr, "Missing argument for i2c bus.\n");
    exit(1);
  }

  /* Make sure to select BME280_I2C_ADDR_PRIM or BME280_I2C_ADDR_SEC as needed */
  dev.dev_id = BME280_I2C_ADDR_PRIM;

  /* dev.dev_id = BME280_I2C_ADDR_SEC; */
  dev.intf = BME280_I2C_INTF;
  dev.read = user_i2c_read;
  dev.write = user_i2c_write;
  dev.delay_ms = user_delay_ms;

  if ((fd = open(argv[1], O_RDWR)) < 0) {
    fprintf(stderr, "Failed to open the i2c bus %s\n", argv[1]);
    return 1;
  }

  if (ioctl(fd, I2C_SLAVE, dev.dev_id) < 0) {
    fprintf(stderr, "Failed to acquire bus access and/or talk to slave.\n");
    return 1;
  }

  /* Initialize the bme280 */
  rslt = bme280_init(&dev);
  if (rslt != BME280_OK) {
    fprintf(stderr, "Failed to initialize the device (code %+d).\n", rslt);
    return 1;
  }

  /* Variable to define the selecting sensors */
  uint8_t settings_sel = 0;

  /* Recommended mode of operation: Indoor navigation */
  dev.settings.osr_h = BME280_OVERSAMPLING_1X;
  dev.settings.osr_p = BME280_OVERSAMPLING_16X;
  dev.settings.osr_t = BME280_OVERSAMPLING_2X;
  dev.settings.filter = BME280_FILTER_COEFF_16;

  settings_sel = BME280_OSR_PRESS_SEL | BME280_OSR_TEMP_SEL | BME280_OSR_HUM_SEL | BME280_FILTER_SEL;

  /* Set the sensor settings */
  rslt = bme280_set_sensor_settings(settings_sel, &dev);
  if (rslt != BME280_OK)
  {
      fprintf(stderr, "Failed to set sensor settings (code %+d).", rslt);

      return rslt;
  }

  /*Calculate the minimum delay required between consecutive measurement based upon the sensor enabled
   *  and the oversampling configuration. */
  req_delay = bme280_cal_meas_delay(&dev.settings);

  printf("Initialised the device\n");

  int measure_size = get_measurements(buffer, sizeof(buffer)-1);
  
  puts(buffer);

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

/*!
 * @brief This function reading the sensor's registers through I2C bus.
 */
int8_t user_i2c_read(uint8_t id, uint8_t reg_addr, uint8_t *data, uint16_t len)
{
    write(fd, &reg_addr, 1);
    read(fd, data, len);

    return 0;
}

/*!
 * @brief This function provides the delay for required time (Microseconds) as per the input provided in some of the
 * APIs
 */
void user_delay_ms(uint32_t period)
{
    /* Milliseconds convert to microseconds */
    usleep(period * 1000);
}

/*!
 * @brief This function for writing the sensor's registers through I2C bus.
 */
int8_t user_i2c_write(uint8_t id, uint8_t reg_addr, uint8_t *data, uint16_t len)
{
    int8_t *buf;

    buf = malloc(len + 1);
    buf[0] = reg_addr;
    memcpy(buf + 1, data, len);
    if (write(fd, buf, len + 1) < len)
    {
        return BME280_E_COMM_FAIL;
    }

    free(buf);

    return BME280_OK;
}
