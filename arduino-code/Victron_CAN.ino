unsigned long Alive_packet_counter = 0;


void setup_can_driver() {
  int rx = D9;
  int tx = D7;
 can_general_config_t general_config = {
        .mode = CAN_MODE_NORMAL,
        .tx_io = (gpio_num_t)tx,
        .rx_io = (gpio_num_t)rx,
        .clkout_io = (gpio_num_t)CAN_IO_UNUSED,
        .bus_off_io = (gpio_num_t)CAN_IO_UNUSED,
        .tx_queue_len = 100,
        .rx_queue_len = 65,
        .alerts_enabled = CAN_ALERT_NONE,
        .clkout_divider = 0};
    can_timing_config_t timing_config = CAN_TIMING_CONFIG_500KBITS();
    can_filter_config_t filter_config = CAN_FILTER_CONFIG_ACCEPT_ALL();
    esp_err_t error;
    gpio_set_direction((gpio_num_t) rx, GPIO_MODE_INPUT);

    error = can_driver_install(&general_config, &timing_config, &filter_config);
    if (error == ESP_OK)
    {
        Serial.println("CAN Driver installation success...");
    }
    else
    {
        Serial.println("CAN Driver installation fail...");
        return;
    }

    // start CAN driver
    error = can_start();
    if (error == ESP_OK)
    {
        Serial.println("CAN Driver start success...");
    }
    else
    {
        Serial.println("CAN Driver start FAILED...");
        return;
    }
}


void CNA_send_Network_alive_msg() {
  Alive_packet_counter ++;
  //Configure message to transmit
  can_message_t message;
  message.identifier = 0x305; //DEZ = 773
  message.flags = CAN_MSG_FLAG_NONE;
  message.data_length_code = 8;
  message.data[0] = Alive_packet_counter & 0xFF;
  message.data[1] = (Alive_packet_counter >>  8) & 0xFF; 
  message.data[2] = (Alive_packet_counter >>  16) & 0xFF; 
  message.data[3] = (Alive_packet_counter >>  24) & 0xFF;
  message.data[4] = 0x00;
  message.data[5] = 0x00; 
  message.data[6] = 0x00; 
  message.data[7] = 0x00; 
             
  //Queue message for transmission
  esp_err_t result = can_transmit(&message, pdMS_TO_TICKS(1000));
  if ( result == ESP_OK) {
      //printf("Message queued for transmission\n");
  } else {
      printf("Failed to queue CNA_send_Network_alive_msg for transmission\n" + result);
  }
}

void Battery_Manufacturer() {

  can_message_t message;
  message.identifier = 0x35E; //DEZ = 862
  message.flags = CAN_MSG_FLAG_NONE;
  message.data_length_code = 8;
//  message.data[0] = 0x50; //P
//  message.data[1] = 0x59; //Y
//  message.data[2] = 0x4c; //L
//  message.data[3] = 0x4f; //O
//  message.data[4] = 0x4e; //N
//  message.data[5] = 0x54; //T
//  message.data[6] = 0x45; //E
//  message.data[7] = 0x43; //C
  // 4c 75 6b 61 73 42 4d 53 LukasBMS
  message.data[0] = 0x4c; //L
  message.data[1] = 0x75; //u
  message.data[2] = 0x6b; //k
  message.data[3] = 0x61; //a
  message.data[4] = 0x73; //s
  message.data[5] = 0x42; //B
  message.data[6] = 0x4d; //M
  message.data[7] = 0x53; //S
             
  //Queue message for transmission
  esp_err_t result = can_transmit(&message, pdMS_TO_TICKS(1000));
  if (result == ESP_OK) {
      printf("Message queued for transmission\n");
  } else {
      printf("Failed to queue Battery_Manufacturer for transmission\n");
      Serial.println(result);
  }
  //Reconfigure alerts to detect Error Passive and Bus-Off error states
//  uint32_t alerts_to_enable = CAN_ALERT_TX_SUCCESS;
//  if (can_reconfigure_alerts(alerts_to_enable, NULL) == ESP_OK) {
//      printf("Alerts reconfigured\n");
//  } else {
//      printf("Failed to reconfigure alerts");
//  }
//  
//  //Block indefinitely until an alert occurs
//  uint32_t alerts_triggered;
//  can_read_alerts(&alerts_triggered, portMAX_DELAY);
}

void Battery_Request() {
  byte request = 0;
  if(charging) {
    request = request | 0x80;
  }
  else {
    request = request | 0x00;
  }
  if(discharging) {
    request = request | 0x40;
  }
  else {
    request = request | 0x00;
  }
  
  can_message_t message;
  message.identifier = 0x35C; //DEZ = 860
  message.flags = CAN_MSG_FLAG_NONE;
  message.data_length_code = 2;
  message.data[0] = request;
  message.data[1] = 0x00; 

             
  //Queue message for transmission
  if (can_transmit(&message, pdMS_TO_TICKS(1000)) == ESP_OK) {
      //printf("Message queued for transmission\n");
  } else {
      printf("Failed to queue Battery_Request for transmission\n");
  }
  
}

void Battery_actual_values_UIt() {

  //Configure message to transmit
  can_message_t message2;
  message2.identifier = 0x356; //DEZ = 854
  message2.flags = CAN_MSG_FLAG_NONE;
  message2.data_length_code = 6;
  message2.data[0] = ((voltage *10) & 0xFF);
  message2.data[1] = (((voltage *10) >> 8) & 0xFF);
  message2.data[2] = (current & 0xFF);
  message2.data[3] = ((current >> 8) & 0xFF);
  message2.data[4] = (CAN_Battery_T1 & 0xFF);
  message2.data[5] = ((CAN_Battery_T1 >> 8) & 0xFF); 

  //Queue message for transmission
  if (can_transmit(&message2, pdMS_TO_TICKS(1000)) == ESP_OK) {
      //printf("Message2 queued for transmission\n");
  } else {
      printf("Failed to queue Battery_actual_values_UIt for transmission\n");
  }
}

void Battery_limits() {

  //Configure message to transmit
  can_message_t message2;
  message2.identifier = 0x351; //DEZ = 849
  message2.flags = CAN_MSG_FLAG_NONE;
  message2.data_length_code =6;
  message2.data[0] = (CAN_Battery_charge_voltage & 0xFF);
  message2.data[1] = ((CAN_Battery_charge_voltage >> 8) & 0xFF);
  message2.data[2] = (CAN_Battery_charge_current_limit & 0xFF);
  message2.data[3] = ((CAN_Battery_charge_current_limit >> 8) & 0xFF);
  message2.data[4] = (CAN_Battery_discharge_current_limit & 0xFF);
  message2.data[5] = ((CAN_Battery_discharge_current_limit >> 8) & 0xFF); 
  //message2.data[6] = (CAN_Battery_discharge_voltage & 0xFF);  
  //message2.data[7] = ((CAN_Battery_discharge_voltage >> 8) & 0xFF); 

  //Queue message for transmission
  if (can_transmit(&message2, pdMS_TO_TICKS(1000)) == ESP_OK) {
      //printf("Message2 queued for transmission\n");
  } else {
      printf("Failed to queue Battery_limits for transmission\n");
  }
}

void Battery_SoC_SoH() {

  //Configure message to transmit
  can_message_t message2;
  message2.identifier = 0x355; //DEZ = 853
  message2.flags = CAN_MSG_FLAG_NONE;
  message2.data_length_code =4;
  message2.data[0] = charge;
  message2.data[1] = 0x00;
  message2.data[2] = CAN_SoH;
  message2.data[3] = 0x00; 

  //Queue message for transmission
  if (can_transmit(&message2, pdMS_TO_TICKS(1000)) == ESP_OK) {
      //printf("Message2 queued for transmission\n");
  } else {
      printf("Failed to queue Battery_SoC_SoH for transmission\n");
  }
}

//void Battery_Error_Warnings() {
//
//  //Configure message to transmit
//  can_message_t message2;
//  message2.identifier = 0x359; //DEZ = 857
//  message2.flags = CAN_MSG_FLAG_NONE;
//  message2.data_length_code =8;
//  message2.data[0] = 0x00;
//  message2.data[1] = 0x00;
//  message2.data[2] = 0x00;
//  message2.data[3] = 0x00;
//  message2.data[4] = 0x01;// number of packs in parallel
//  message2.data[5] = 0x56; // Product ID, Pylontech: 0x50
//  message2.data[6] = 0x45;//product_id_2; // 0x4E
//  message2.data[7] = 0x01;// online adress of packs in parallel
//  
//  //Queue message for transmission
//  if (can_transmit(&message2, pdMS_TO_TICKS(1000)) == ESP_OK) {
//      //printf("Message2 queued for transmission\n");
//  } else {
//      printf("Failed to queue Battery_Error_Warnings for transmission\n");
//  }
//}

//Replacing Battery Warnings with new spec
void Battery_Error_Warnings() {

  //Configure message to transmit
  can_message_t message2;
  message2.identifier = 0x35A; 
  message2.flags = CAN_MSG_FLAG_NONE;
  message2.data_length_code =8;
  message2.data[0] = 0x00;
  message2.data[1] = 0x00;
  message2.data[2] = 0x00;
  message2.data[3] = 0x00;
  message2.data[4] = 0x00;
  message2.data[5] = 0x00;
  message2.data[6] = 0x00;
  message2.data[7] = 0x00;
  
  //Queue message for transmission
  if (can_transmit(&message2, pdMS_TO_TICKS(1000)) == ESP_OK) {
      //printf("Message2 queued for transmission\n");
  } else {
      printf("Failed to queue Battery_Error_Warnings for transmission\n");
  }
}
