# Projeto ESP32 + MQTT

Este projeto tem como objetivo realizar a conexão da ESP32 com um broker MQTT, permitindo o controle do LED da placa por meio de mensagens publicadas em um tópico específico.

## 🎯 Objetivo

Conectar a ESP32 a um broker MQTT e controlar um LED da seguinte forma:

- Ao publicar o valor `1` no tópico, o LED da ESP32 acende.
- Ao publicar o valor `0`, o LED da ESP32 apaga.

## 🧰 Tecnologias Utilizadas

- [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/)
- [Mosquitto](https://mosquitto.org/) (broker MQTT local)
- Linguagem C
- Protocolo MQTT 5.0
- [MQTTX](https://mqttx.app/) (cliente MQTT)

---

## ⚙️ Configuração do Broker Mosquitto (Local)

Se desejar utilizar um broker local com o Mosquitto, siga os passos abaixo:

1. Instale o Mosquitto (disponível para Linux, Windows e macOS).
2. Localize o arquivo `mosquitto.conf` na pasta de instalação.
3. Edite o arquivo e adicione as seguintes linhas:

   ```conf
   listener 1883
   allow_anonymous true
   ```

4. Salve o arquivo.
5. Inicie o broker com o comando:

   ```bash
   mosquitto -c "C:\diretoro\raiz\do\mosquitto.conf" -v
   ```

**Importante:** A ESP32 e o broker devem estar na **mesma rede local** para que a conexão funcione corretamente.

---

## 🚀 Como Rodar o Projeto

### 1. Clone o repositório

```bash
git clone https://github.com/MatheusH2021/mqtt-projeto-SE.git
cd nome-do-repositorio
```

### 2. Configure sua rede Wi-Fi

Edite o arquivo `sdkconfig` (será criado após o primeiro build) com os seguintes campos:

```bash
CONFIG_EXAMPLE_WIFI_SSID="nome_da_sua_rede_wifi"
CONFIG_EXAMPLE_WIFI_PASSWORD="senha_da_sua_rede"
```

> Se o arquivo `sdkconfig` ainda não existir, execute um build inicial com `idf.py build` para que ele seja gerado.

### 3. Configure o endereço do broker

No arquivo `main/app_main.c`, modifique a seguinte linha:

```c
#define MQTT_BROKER_ADDR "mqtt://<SEU_BROKER>"
```

Substitua `<SEU_BROKER>` pelo IP do seu broker local ou por um broker público (ex: `mqtt://192.168.0.100`).

### 4. Compile e faça o flash do projeto na ESP32

```bash
idf.py build
idf.py flash
```

---

## 🧪 Como Testar

1. Inicie o broker Mosquitto:

   ```bash
   mosquitto -c "C:\diretorio\raiz\do\mosquitto.conf" -v
   ```

2. No MQTTX (cliente MQTT):

   - Clique em **"+ New Connection"**
   - Defina um nome qualquer
   - No campo **Host**, insira o IP do seu broker local (ex: `192.168.0.100`)
   - Clique em **Connect**

3. Crie uma nova assinatura (**+ New Subscription**) com o tópico:

   ```
   /ifpe/ads/embarcados/esp32/led
   ```

4. Ligue sua ESP32 (com o firmware já gravado). Ela se conectará automaticamente ao Wi-Fi e ao broker.  
   Se tudo ocorrer normalmente, no cliente MQTTX você deve receber as seguintes mensagens:

   ```
   {"dispositivo": "esp32", "status": "conectado"}
   ```

   e

   ```
   {"dispositivo": "esp32", "topico": "/ifpe/ads/embarcados/esp32/led", "status": "Inscrito"}
   ```

5. Para testar:

   - Envie o número `1` no campo de publicação → o LED acende.
   - Envie o número `0` → o LED apaga.

---

## 📝 Observações

- Certifique-se de que a ESP32 e o dispositivo que executa o Mosquitto estão na mesma rede.
- O tópico utilizado no projeto é fixo: `/ifpe/ads/embarcados/esp32/led`.
- Caso tenha problemas para conectar a ESP32 ao seu broker local, verifique se o firewall do seu computador está permitindo que o Mosquitto se comunique pela rede.

---
