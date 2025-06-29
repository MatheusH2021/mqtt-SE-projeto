/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include "driver/gpio.h"

static const char *TAG = "mqtt5_example";

#define MQTT_BROKER_ADDR "mqtt://<SEU BROKER AQUI>" // Se for local seu broker será: mqtt://SeuIp:1883, se for online será a url de destino
#define MQTT_BROKER_TOPIC "/ifpe/ads/embarcados/esp32/led"
#define LED_PIN_NUM 2

static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

static esp_mqtt5_user_property_item_t user_property_arr[] = {
        {"board", "esp32"},
        {"u", "user"},
        {"p", "password"}
    };

#define USE_PROPERTY_ARR_SIZE   sizeof(user_property_arr)/sizeof(esp_mqtt5_user_property_item_t)

static esp_mqtt5_disconnect_property_config_t disconnect_property = {
    .session_expiry_interval = 60,
    .disconnect_reason = 0,
};

static void print_user_property(mqtt5_user_property_handle_t user_property)
{
    if (user_property) {
        uint8_t count = esp_mqtt5_client_get_user_property_count(user_property);
        if (count) {
            esp_mqtt5_user_property_item_t *item = malloc(count * sizeof(esp_mqtt5_user_property_item_t));
            if (esp_mqtt5_client_get_user_property(user_property, item, &count) == ESP_OK) {
                for (int i = 0; i < count; i ++) {
                    esp_mqtt5_user_property_item_t *t = &item[i];
                    ESP_LOGI(TAG, "key is %s, value is %s", t->key, t->value);
                    free((char *)t->key);
                    free((char *)t->value);
                }
            }
            free(item);
        }
    }
}

/*
 * @brief Event handler registered to receive MQTT events
 *
 *  This function is called by the MQTT client event loop.
 *
 * @param handler_args user data registered to the event.
 * @param base Event base for the handler(always MQTT Base in this example).
 * @param event_id The id for the received event.
 * @param event_data The data for the event, esp_mqtt_event_handle_t.
 */

static void mqtt5_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32, base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;

    ESP_LOGD(TAG, "free heap size is %" PRIu32 ", minimum %" PRIu32, esp_get_free_heap_size(), esp_get_minimum_free_heap_size());
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");

        const char *json_msg_conect = "{\"dispositivo\": \"esp32\", \"status\": \"conectado\"}";
        // Publica uma simples mensagem no broker em formato JSON de conexão ao broker (Informa que a esp32 foi conectada)
        msg_id = esp_mqtt_client_publish(client, MQTT_BROKER_TOPIC, json_msg_conect, 0, 1, 0);
        ESP_LOGI(TAG, "Mensagem de conexão enviada com sucesso, msg_id=%d", msg_id);

        // Inscreve a esp32 no topico "/ifpe/ads/embarcados/esp32/led"
        msg_id = esp_mqtt_client_subscribe(client, MQTT_BROKER_TOPIC, 1);
        ESP_LOGI(TAG, "Esp32 Inscrito no topico %s", MQTT_BROKER_TOPIC);
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        print_user_property(event->property->user_property);
        break;
    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);

        // Gera a mensagem de inscrição ao tópico esolhido
        char json_msg_sub[256];  // Buffer para armazenar a string final
        snprintf(json_msg_sub, sizeof(json_msg_sub), "{\"dispositivo\": \"esp32\", \"topico\": \"%s\", \"status\": \"Inscrito\"}", MQTT_BROKER_TOPIC);

        // Publica uma mensagem no broker em formato JSON informando que a esp32 está inscrita no tópico
        msg_id = esp_mqtt_client_publish(client, MQTT_BROKER_TOPIC, json_msg_sub, 0, 0, 0);
        ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        print_user_property(event->property->user_property);
        esp_mqtt5_client_set_user_property(&disconnect_property.user_property, user_property_arr, USE_PROPERTY_ARR_SIZE);
        esp_mqtt5_client_set_disconnect_property(client, &disconnect_property);
        esp_mqtt5_client_delete_user_property(disconnect_property.user_property);
        disconnect_property.user_property = NULL;
        esp_mqtt_client_disconnect(client);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        print_user_property(event->property->user_property);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        // Aqui realizamos a logica de gerenciamento do LED
        // quando recebe 1- Liga o LED
        // quando recebe 0- Desliga o LED

        // Verifica se o topico recebido é igual ao tópico definido
        if (event->topic_len == strlen(MQTT_BROKER_TOPIC) &&
            memcmp(event->topic, MQTT_BROKER_TOPIC, event->topic_len) == 0) {
            
            // Loga o tópico e a mensagem recebida
            ESP_LOGI(TAG, "Mensagem recebida do tópico %.*s: %.*s",
                    event->topic_len, event->topic,
                    event->data_len, event->data);

            // Verifica se há pelo menos um byte de dado antes de acessar event->data[0]
            if (event->data_len > 0) {
                // Pega o primeiro caractere da mensagem (usado como comando)
                char cmd = event->data[0];

                // Analisa o comando usando switch-case 
                switch (cmd) {
                    case '1':
                        // Neste case, caso o comando seja 1- Ativa o LED, Loga a ação realizada
                        ESP_LOGI(TAG, "Ativando LED da esp32.");
                        gpio_set_level(LED_PIN_NUM, 1);
                        break;
                    case '0':
                        // Neste case, caso o comando seja 0- Desliga o LED, Loga a ação realizada
                        ESP_LOGI(TAG, "Desativando LED da esp32.");
                        gpio_set_level(LED_PIN_NUM, 0);
                        break;
                    default:
                        // Caso o comando seja diferente de 1 ou 0, loga apenas "comando desconhecido"
                        ESP_LOGW(TAG, "Comando desconhecido: %c", cmd);
                        break;
                }
            } else {
                // Caso não tenha byte, Loga a mensagem de dados vazios
                ESP_LOGW(TAG, "Dados vazios recebidos no tópico.");
            }

        } else {
            ESP_LOGI(TAG, "Mensagem recebida de tópico não monitorado: %.*s",
                    event->topic_len, event->topic);
        }
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        print_user_property(event->property->user_property);
        ESP_LOGI(TAG, "MQTT5 return code is %d", event->error_handle->connect_return_code);
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno",  event->error_handle->esp_transport_sock_errno);
            ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));
        }
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

static void mqtt5_app_start(void)
{
    // Configura as opções do protocolo MQTT
    esp_mqtt5_connection_property_config_t connect_property = {
        .session_expiry_interval = 10,
        .maximum_packet_size = 1024,
        .receive_maximum = 65535,
        .topic_alias_maximum = 2,
        .request_resp_info = true,
        .request_problem_info = true,
        .will_delay_interval = 10,
        .payload_format_indicator = true,
        .message_expiry_interval = 10,
        .response_topic = MQTT_BROKER_TOPIC,
        .correlation_data = "123456",
        .correlation_data_len = 6,
    };

    esp_mqtt_client_config_t mqtt5_cfg = {
        .broker.address.uri = MQTT_BROKER_ADDR, // Broker usado para conexão
        .session.protocol_ver = MQTT_PROTOCOL_V_5, // Versão do protocolo MQTT usado
        .session.last_will.topic = MQTT_BROKER_TOPIC, // Tópico a ser publicado a mensagem de desconexão inesperada
        .session.last_will.msg = "offline",         // Mensagem a ser publicada caso haja desconexão inesperada
        .session.last_will.msg_len = strlen("offline"), // Tamanho da mensagem
        .session.last_will.qos = 1,
    };

#if CONFIG_BROKER_URL_FROM_STDIN
    char line[128];

    if (strcmp(mqtt5_cfg.uri, "FROM_STDIN") == 0) {
        int count = 0;
        printf("Please enter url of mqtt broker\n");
        while (count < 128) {
            int c = fgetc(stdin);
            if (c == '\n') {
                line[count] = '\0';
                break;
            } else if (c > 0 && c < 127) {
                line[count] = c;
                ++count;
            }
            vTaskDelay(10 / portTICK_PERIOD_MS);
        }
        mqtt5_cfg.broker.address.uri = line;
        printf("Broker url: %s\n", line);
    } else {
        ESP_LOGE(TAG, "Configuration mismatch: wrong broker url");
        abort();
    }
#endif /* CONFIG_BROKER_URL_FROM_STDIN */

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt5_cfg);

    /* Set connection properties and user properties */
    esp_mqtt5_client_set_user_property(&connect_property.user_property, user_property_arr, USE_PROPERTY_ARR_SIZE);
    esp_mqtt5_client_set_user_property(&connect_property.will_user_property, user_property_arr, USE_PROPERTY_ARR_SIZE);
    esp_mqtt5_client_set_connect_property(client, &connect_property);

    /* If you call esp_mqtt5_client_set_user_property to set user properties, DO NOT forget to delete them.
     * esp_mqtt5_client_set_connect_property will malloc buffer to store the user_property and you can delete it after
     */
    esp_mqtt5_client_delete_user_property(connect_property.user_property);
    esp_mqtt5_client_delete_user_property(connect_property.will_user_property);

    /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt5_event_handler, NULL);
    esp_mqtt_client_start(client);
}

void app_main(void)
{
    //Habilitando o gpio como saida
    gpio_config_t io_config = {
        .pin_bit_mask = (1ULL << LED_PIN_NUM),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_config);

    // Inicializando o gpio do LED para iniciar como 0 (desligado)
    gpio_set_level(LED_PIN_NUM, 0);

    ESP_LOGI(TAG, "[APP] Startup..");
    ESP_LOGI(TAG, "[APP] Free memory: %" PRIu32 " bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("mqtt_client", ESP_LOG_VERBOSE);
    esp_log_level_set("mqtt_example", ESP_LOG_VERBOSE);
    esp_log_level_set("transport_base", ESP_LOG_VERBOSE);
    esp_log_level_set("esp-tls", ESP_LOG_VERBOSE);
    esp_log_level_set("transport", ESP_LOG_VERBOSE);
    esp_log_level_set("outbox", ESP_LOG_VERBOSE);

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */
    ESP_ERROR_CHECK(example_connect());

    mqtt5_app_start();
}
