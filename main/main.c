#include <stdio.h>
#include <stdlib.h>

//BIBLIOTECAS DO FREERTOS
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "driver/gpio.h" //BIBLIOTECA DO MÓDULO GPIO
#include "driver/i2c.h"  //BIBLIOTECA DO I2C
#include "esp_log.h" //BIBLIOTECA DE LOGS DE DEBUGGER
#include "driver/ledc.h"  //BIBLIOTECA DO M´DULO PWM
#include "nvs_flash.h"

#include <dht.h> // BIBLIOTECA DO SENSOR
#include "i2c-lcd1602.h" // BIBLIOTECA DO DISPLAY

//BIBLIOTECAS DA COMUNICAÇÃO SEM FIO
#include "wifi.h"
#include "MQTT_lib.h"
#include <esp_http_server.h>
#include "esp_netif.h"
#include <lwip/sockets.h>
#include <lwip/sys.h>
#include <lwip/api.h>
#include <lwip/netdb.h>

//DEFINIÇÃO DE PARÂMETROS PARA O DISPLAY LCD COM I2C
#define I2C_MASTER_SCL_IO 22 //GPIO DO SCL
#define I2C_MASTER_SDA_IO 21 //GPIO DO SDA
#define I2C_MASTER_NUM I2C_NUM_0 //DIVER I2C UTILIZADO
#define I2C_MASTER_FREQ_HZ 100000
#define I2C_SLAVE_ADDR 0x27 //ENDEREÇO DO DISPLAY
#define LCD_COLS 16 // Número de colunas do display LCD 
#define LCD_ROWS 2 // Número de linhas do display LCD

//DEFINIÇÃO DE PINO PARA O SENSOR DE TEMPERATURA
#define DHT_PIN GPIO_NUM_18 //GPIO DO SENSOR

//CRIAÇÃO DE MUTEX E SEMÁFORO PARA CONTROLE DE EXECUÇÃO ENTRE AS TAREFAS
SemaphoreHandle_t xTempMutex, xSemaphoreDisplay, xSemaphorePWM;

void i2c_master_init(); // PROTÓTIPO DA FUNÇÃO PARA INICIALIZAR O DRIVE I2C

float calcularIndiceDeCalor(); // PROTÓTIPOD A FUNÇÃO PARA CALCULAR O ÍNDICE DE CALOR

const char* determinarConfortoTermico(); // PROTÓTIPOD A FUNÇÃO PARA DETERMINAR O CONFORTO TÉRMICO

void pwm(); // PROTÓTIO DA FUNÇÃO DE INICIALIZAÇÃO DO PWM

void vTemperatura(void *pvParameter); //TAREFA PARA LER TEMPERATURA E HUMIDADE
void vDisplay(void *pvParameter); //TAREFA PARA ESCREVER NO DISPLAY
void vSetPWM(void *pvParameter); //TAREFA PARA AJUSTAR PWM
void vMQTT(void *pvParameter); //TAREFA PARA ESCREVER NO BROKER MQTT
void vHTTP(void *pvParameter); //TAREFA PARA ESCREVER NO SITE

//DECLARAÇÃO DAS CONFIGURAÇÕES DA WEBPAGE
char html_page[] = "<!DOCTYPE HTML><html>\n"
                   "<head>\n"
                   "  <title>ELE0629 - AR CONDICIONADO</title>\n"
                   "  <meta http-equiv=\"refresh\" content=\"10\">\n"
                   "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n"
                   "  <link rel=\"stylesheet\" href=\"https://use.fontawesome.com/releases/v5.7.2/css/all.css\" integrity=\"sha384-fnmOCqbTlWIlj8LyTjo7mOUStjsKC4pOpQbqyi7RrhN7udi9RwhKkMHpvLbHG9Sr\" crossorigin=\"anonymous\">\n"
                   "  <link rel=\"icon\" href=\"data:,\">\n"
                   "  <style>\n"
                   "    html {font-family: Arial; display: inline-block; text-align: center;}\n"
                   "    p {  font-size: 1.2rem;}\n"
                   "    body {  margin: 0;}\n"
                   "    .topnav { overflow: hidden; background-color: #241d4b; color: white; font-size: 1.7rem; }\n"
                   "    .content { padding: 20px; }\n"
                   "    .card { background-color: white; box-shadow: 2px 2px 12px 1px rgba(140,140,140,.5); }\n"
                   "    .cards { max-width: 700px; margin: 0 auto; display: grid; grid-gap: 2rem; grid-template-columns: repeat(auto-fit, minmax(300px, 1fr)); }\n"
                   "    .reading { font-size: 2.0rem; }\n"
                   "    .card.temperature { color: #0e7c7b; }\n"
                   "    .card.humidity { color: #17bebb; }\n"
                   "    .card.conforto { color:rgb(0, 80, 78);}\n"
                   "  </style>\n"
                   "</head>\n"
                   "<body>\n"
                   "  <div class=\"topnav\">\n"
                   "    <h3>ELE0629 - AR CONDICIONADO</h3>\n"
                   "  </div>\n"
                   "  <div class=\"content\">\n"
                   "    <div class=\"cards\">\n"
                   "      <div class=\"card temperature\">\n"
                   "        <h4><i class=\"fas fa-thermometer-half\"></i> TEMPERATURA</h4><p><span class=\"reading\">%.2f&deg;C</span></p>\n"
                   "      </div>\n"
                   "      <div class=\"card humidity\">\n"
                   "        <h4><i class=\"fas fa-tint\"></i> UMIDADE</h4><p><span class=\"reading\">%.2f</span> &percnt;</span></p>\n"
                   "      </div>\n"
                   "      <div class=\"card conforto\">\n"
                   "        <h4><i class=\"fas fa-bed\"></i> CONFORTO</h4><p><span class=\"reading\">%s</p>\n"
                   "      </div>\n"
                   "    </div>\n"
                   "  </div>\n"
                   "</body>\n"
                   "</html>";

float umidade;
float temperatura;

static const char *TASK_TEMP = "TEMPERATURA";
static const char *TASK_DISPLAY = "DISPLAY";
static const char *TASK_PWM = "PWM";
static const char *TASK_MQTT = "MQTT";
static const char *TASK_HTTP = "HTTP";

void app_main(void) {

    nvs_flash_init();
    wifi_init_sta();

    xTempMutex = xSemaphoreCreateMutex();
    xSemaphoreDisplay = xSemaphoreCreateBinary();
    xSemaphorePWM = xSemaphoreCreateBinary();
  
  // Criação das tarefas
    xTaskCreatePinnedToCore(vTemperatura, "vTemperatura", 2048, NULL, 1, NULL, 0);
    xTaskCreatePinnedToCore(vDisplay, "vDisplay", 2048, NULL, 3, NULL, 0);
    xTaskCreatePinnedToCore(vSetPWM, "vSetPWM", 2048, NULL, 5, NULL, 0);
    xTaskCreatePinnedToCore(vMQTT, "vMQTT", 4096, NULL, 1, NULL, 1);
    xTaskCreatePinnedToCore(vHTTP, "vHTTP", 4096, NULL, 3, NULL, 1);
    
}

//FUNÇÃO PARA INICIALIZAR O DRIVE I2C
void i2c_master_init(){
  i2c_config_t conf={
    .mode = I2C_MODE_MASTER,
    .sda_io_num = I2C_MASTER_SDA_IO,
    .sda_pullup_en = GPIO_PULLUP_ENABLE,
    .scl_io_num = I2C_MASTER_SCL_IO,
    .scl_pullup_en = GPIO_PULLUP_ENABLE,
    .master.clk_speed = I2C_MASTER_FREQ_HZ,
  };
  ESP_ERROR_CHECK(i2c_param_config(I2C_MASTER_NUM, &conf));
  ESP_ERROR_CHECK(i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0));
  
};

//FUNÇÃO PARA CALCULAR O ÍNDICE DE TEMPERATURA
float calcularIndiceDeCalor() { 
  return -8.784695 + 1.61139411 * temperatura + 2.338549 * umidade - 0.14611605 * temperatura * umidade 
  - 0.012308094 * (temperatura * temperatura) - 0.016424828 * (umidade * umidade)
   + 0.002211732 * (temperatura * temperatura) * umidade + 0.00072546 * temperatura * (umidade * umidade) 
   - 0.000003582 * (temperatura * temperatura) * (umidade * umidade);
   };

//FUNÇÃO PARA DETERMINAR O CONFORTO TÉRMICO
const char* determinarConfortoTermico() { 
  float indiceDeCalor = calcularIndiceDeCalor(); 
  if (indiceDeCalor < 27.0) { 
    return "CONFORTAVEL"; }
   else if (indiceDeCalor < 32.0) { 
    return "MOD. DESCONF."; 
    } else if (indiceDeCalor < 39.0) { 
        return "DESCONFORTAVEL"; } 
      else if (indiceDeCalor < 46.0) { 
        return "PERIGOSO"; } 
        else { return "EXTR. PERIGOSO"; 
        } ;
}

//TAREFA PARA LER TEMPERATURA
void vTemperatura(void *pvParameter) {
  
  
    while(1) {
      if(xSemaphoreTake(xTempMutex, portMAX_DELAY) == pdTRUE){
        
        ESP_LOGI(TASK_TEMP, "LEITURA DE TEMPERATURA EXECUTADA.");  
       
        //OBTENÇÃO DOS DADOS DO SENSOR
        ESP_ERROR_CHECK(dht_read_float_data(DHT_TYPE_DHT11, DHT_PIN,&umidade, &temperatura));
        ESP_LOGI(TASK_TEMP,"--------------------");
        ESP_LOGI(TASK_TEMP,"Temperatura: %.1f°C", temperatura);
        ESP_LOGI(TASK_TEMP,"Umidade: %.1f%%", umidade);
        ESP_LOGI(TASK_TEMP,"--------------------");

        xSemaphoreGive(xTempMutex);
        xSemaphoreGive(xSemaphoreDisplay);
      }else{
         ESP_LOGE(TASK_TEMP, "LEITURA DE TEMPERATURA NAO EXECUTADA.");
      }
      vTaskDelay(pdMS_TO_TICKS(50));
  }
}

//TAREFA PARA ESCREVER NO DISPLAY
void vDisplay(void *pvParameter) {

  //INICIALIZAR O DRIVE I2C
  i2c_master_init();

  //ALOCAR A INICIALIZAR A ESTRUTURA DE INFORMAÇÕES SMBUS
  smbus_info_t * smbus_info = smbus_malloc();
  ESP_ERROR_CHECK(smbus_init(smbus_info, I2C_MASTER_NUM, I2C_SLAVE_ADDR));
  ESP_ERROR_CHECK(smbus_set_timeout(smbus_info, 1000 / portTICK_PERIOD_MS));

  //ALUCAR A INICIALIZAR AS INFORMAÇÕES DO DISPLAY
  i2c_lcd1602_info_t * lcd_info = i2c_lcd1602_malloc();
  ESP_ERROR_CHECK(i2c_lcd1602_init(lcd_info, smbus_info, true, LCD_ROWS, LCD_COLS, LCD_COLS));

  char buffer1[17];// PARA ARMAZENAR A STRING A SER EXIBIDA

  while(1) {
    if(xSemaphoreTake(xSemaphoreDisplay, portMAX_DELAY) == pdTRUE){
      if(xSemaphoreTake(xTempMutex, portMAX_DELAY) == pdTRUE){
        ESP_LOGI(TASK_DISPLAY, "ESCRITA DISPLAY EXECUTADA.");  

        // Configurar o LCD
        ESP_ERROR_CHECK(i2c_lcd1602_clear(lcd_info)); 
        ESP_ERROR_CHECK(i2c_lcd1602_move_cursor(lcd_info, 0, 0)); 
        sprintf(buffer1, "TEMP.: %.1f\xDF""C", temperatura);
        ESP_ERROR_CHECK(i2c_lcd1602_write_string(lcd_info, buffer1)); 
        ESP_ERROR_CHECK(i2c_lcd1602_move_cursor(lcd_info, 0, 1)); 
        ESP_ERROR_CHECK(i2c_lcd1602_write_string(lcd_info, determinarConfortoTermico())); 

        xSemaphoreGive(xTempMutex);
        xSemaphoreGive(xSemaphorePWM);
      }else{
        ESP_LOGE(TASK_DISPLAY, "ERRO MUTEX: LEITURA DE DISPLAY NAO EXECUTADA.");
      }

    }else{
        ESP_LOGE(TASK_DISPLAY, "ERRO SEMAFORO: LEITURA DE DISPLAY NAO EXECUTADA.");
  }
}
}

//TAREFA PARA ATUALIZAR O PWM
void vSetPWM(void *pvParameter){

    pwm();

    float duty = 0;

    while(1){
       if(xSemaphoreTake(xSemaphorePWM, portMAX_DELAY) == pdTRUE){
          if(xSemaphoreTake(xTempMutex, portMAX_DELAY) == pdTRUE){
            duty =((temperatura*0.0233)-0.4)*1024; // CALCULO DO NOVO VALOR DO DUTY CICLE COM BASE NA TEMPERATURA
            if (duty >= 1024){
              duty = 1000;
            };
            ESP_ERROR_CHECK(ledc_set_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0, (int)duty));//ATUALIZAR O DUTY CICLE
            ledc_update_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0);
            ESP_LOGI(TASK_PWM, "DUTY CICLE = %f.",duty);
            xSemaphoreGive(xTempMutex);
        }else{
          ESP_LOGE(TASK_DISPLAY, "ERRO MUTEX: ATUALIZACAO PWM NAO EXECUTADO.");
        }  
        }else{
          ESP_LOGE(TASK_DISPLAY, "ERRO SEMAFORO: ATUALIZACAO PWM NAO EXECUTADO.");
        }  
    };
};

//DECLARAÇÃO DA FUNÇÃO PWM
void pwm(){

    //PWM TIMER LEDC
    ledc_timer_config_t timer_conf ={
        .speed_mode = LEDC_HIGH_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 1,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .clk_cfg = LEDC_AUTO_CLK
    };

    ESP_ERROR_CHECK(ledc_timer_config(&timer_conf));
    
    //PWM CHANNEL LEDC
    ledc_channel_config_t channel_config = {
        .channel  = LEDC_CHANNEL_0,
        .speed_mode = LEDC_HIGH_SPEED_MODE,
        .timer_sel = LEDC_TIMER_0,
        .intr_type = LEDC_INTR_DISABLE,
        .gpio_num = GPIO_NUM_15,
        .duty = 511
    };

    ESP_ERROR_CHECK(ledc_channel_config(&channel_config));

};

//TAREFA PARA ENVIO DOS DADOS VIA MQTT
void vMQTT(void *pvParameter){

    mqtt_start();
  while(true){
    if(xSemaphoreTake(xTempMutex, portMAX_DELAY) == pdTRUE){
      char msg1[50];
      char msg2[50];
      char *msg3;

      snprintf(msg1, sizeof(msg1), "Temp: %.2f °C", temperatura);
      mqtt_publish("ELE0629/Temperatura", msg1, 0, 0);
      ESP_LOGI(TASK_MQTT, "PUBLICACAO TEMPERATURA EXECUTADA."); 

      snprintf(msg2, sizeof(msg2), "Umid: %.2f%%", umidade);
      mqtt_publish("ELE0629/Umidade", msg2, 0, 0);
      ESP_LOGI(TASK_MQTT, "PUBLICACAO UMIDADE EXECUTADA.");

      msg3 = determinarConfortoTermico();
      mqtt_publish("ELE0629/Conforto", msg3, 0, 0);
      ESP_LOGI(TASK_MQTT, "PUBLICACAO CONFORTO EXECUTADA.");
      
      xSemaphoreGive(xTempMutex);
      }else{
        ESP_LOGE(TASK_MQTT, "PUBLICACAO NAO EXECUTADA.");
      }
      vTaskDelay(pdMS_TO_TICKS(2000)); 
    }
}; 

esp_err_t send_web_page(httpd_req_t *req)
{
    int response;
    char *msg;
    msg = determinarConfortoTermico();
    
    char response_data[sizeof(html_page) + 50];
    memset(response_data, 0, sizeof(response_data));
    sprintf(response_data, html_page, temperatura, umidade, msg);
    response = httpd_resp_send(req, response_data, HTTPD_RESP_USE_STRLEN);
   
    xSemaphoreGive(xTempMutex); 
    return response;
}

esp_err_t get_req_handler(httpd_req_t *req)
{
    if (xSemaphoreTake(xTempMutex, portMAX_DELAY)) { 
            return send_web_page(req);
        }else{
                return 0;
            }
    
}

httpd_uri_t uri_get = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = get_req_handler,
    .user_ctx = NULL};

httpd_handle_t setup_server(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;

    if (httpd_start(&server, &config) == ESP_OK)
    {
        httpd_register_uri_handler(server, &uri_get);
    }

    return server;
}

void vHTTP(void *pvParameter) { 
    

   if (wifi_conect_status() == 1)
    {
        setup_server();
        ESP_LOGI(TASK_HTTP, "Web Server is up and running\n");
    }
     else
     {
        ESP_LOGI(TASK_HTTP, "Failed to connected with Wi-Fi, check your network Credentials\n");
     }

    while (1) { 
        vTaskDelay(pdMS_TO_TICKS(1000)); 
    } 
}