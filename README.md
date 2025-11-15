# Controle_Temperatura

Projeto de controle de velocidade de um motor usando PWM (Pulse Width Modulation) em função da temperatura, direcionado para a plataforma ESP32.

---

## 🎯 Objetivo

O objetivo principal deste projeto é implementar um sistema que ajusta automaticamente a velocidade de um motor (provavelmente um cooler ou ventilador) com base em leituras de temperatura, utilizando a modulação por largura de pulso (PWM) para o controle da velocidade.

---

## 💻 Plataforma Alvo

* **ESP32**

---

## 🛠️ Tecnologias Utilizadas

* **Linguagem Principal:** C (98.0%)
* **Sistema de Build:** CMake (1.3%)

---

## 📁 Estrutura do Repositório

/
|-- .devcontainer/  # Configurações para ambientes de desenvolvimento em container
|-- .vscode/        # Configurações do editor Visual Studio Code
|-- build/          # (Geralmente contém os arquivos de compilação)
|-- components/     # (Componentes específicos do projeto ESP-IDF)
|-- main/           # Código fonte principal da aplicação
|-- CMakeLists.txt  # Arquivo principal do CMake para build
|-- README.md       # Este arquivo

---

## 🚀 Como Compilar e Usar (Sugestão)

Este projeto parece utilizar o **ESP-IDF** (Espressif IoT Development Framework).

1.  **Clone o repositório:**
    ```bash
    git clone [https://github.com/liongex/Controle_Temperatura.git](https://github.com/liongex/Controle_Temperatura.git)
    cd Controle_Temperatura
    ```

2.  **Configure o ambiente ESP-IDF:**
    Certifique-se de que o ESP-IDF esteja instalado e configurado corretamente no seu sistema.
    (Siga o [guia oficial de instalação do ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html)).

3.  **Configure o projeto (se necessário):**
    ```bash
    idf.py menuconfig
    ```
    (Use esta etapa para ajustar configurações específicas do projeto, como pinos de GPIO para o sensor de temperatura e o motor).

4.  **Compile o projeto:**
    ```bash
    idf.py build
    ```

5.  **Grave no ESP32:**
    Conecte seu ESP32 e execute:
    ```bash
    idf.py -p /dev/ttyUSB0 flash monitor
    ```
    (Substitua `/dev/ttyUSB0` pela porta serial correta do seu dispositivo).
