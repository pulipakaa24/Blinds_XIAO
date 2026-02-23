
    // 3. Post Event to System Loop
    battery_data_t data = { .soc = soc, .voltage = voltage };
    
    esp_event_post(BATTERY_EVENTS, BATTERY_EVENT_UPDATE, &data, sizeof(data), 0);
    
    // Optional: Post warnings
    if (soc < 20.0) {
      esp_event_post(BATTERY_EVENTS, BATTERY_EVENT_LOW, NULL, 0, 0);
    }