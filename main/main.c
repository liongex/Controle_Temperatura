#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"
#include "driver/i2c.h"  //BIBLIOTECA DO I2C
#include "esp_log.h"
#include "driver/ledc.h"
#include "freertos/semphr.h"

#include <dht.h> // BIBLIOTECA DO SENSOR
#include "i2c-lcd1602.h" // BIBLIOTECA DO DISPLAY



#define I2C_MASTER_SCL_IO 22 //GPIO DO SCL
#define I2C_MASTER_SDA_IO 21 //GPIO DO SDA
#define I2C_MASTER_NUM I2C_NUM_0 //DIVER I2C UTILIZADO
#define I2C_MASTER_FREQ_HZ 100000
#define I2C_SLAVE_ADDR 0x27 //ENDEREÇO DO DISPLAY
#define LCD_COLS 16 // Número de colunas do display LCD 
#define LCD_ROWS 2 // Número de linhas do display LCD

#define DHT_PIN GPIO_NUM_18 //GPIO DO SENSOR

SemaphoreHandle_t xTempMutex, xSemaphoreDisplay, xSemaphorePWM;

void i2c_master_init(); // PROTÓTIPO DA FUNÇÃO PARA INICIALIZAR O DRIVE I2C

float calcularIndiceDeCalor(); // PROTÓTIPOD A FUNÇÃO PARA CALCULAR O ÍNDICE DE CALOR

const char* determinarConfortoTermico(); // PROTÓTIPOD A FUNÇÃO PARA DETERMINAR O CONFORTO TÉRMICO

void pwm(); // PROTÓTIO DA FUNÇÃO DE INICIALIZAÇÃO DO PWM

void vTemperatura(void *pvParameter); //TAREFA PARA LER TEMPERATURA E HUMIDADE
void vDisplay(void *pvParameter); //TAREFA PARA ESCREVER NO DISPLAY
void vSetPWM(void *pvParameter); //TAREFA PARA AJUSTAR PWM

float umidade;
float temperatura;

static const char *TASK_TEMP = "TEMPERATURA";
static const char *TASK_DISPLAY = "DISPLAY";
static const char *TASK_PWM = "PWM";

void app_main(void) {

    xTempMutex = xSemaphoreCreateMutex();
    xSemaphoreDisplay = xSemaphoreCreateBinary();
    xSemaphorePWM = xSemaphoreCreateBinary();
  
  // Criação das tarefas
    xTaskCreate(vTemperatura, "vTemperatura", 2048, NULL, 1, NULL);
    xTaskCreate(vDisplay, "vDisplay", 2048, NULL, 3, NULL);
    xTaskCreate(vSetPWM, "vSetPWM", 2048, NULL, 5, NULL);
    
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