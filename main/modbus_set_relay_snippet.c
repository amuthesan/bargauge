
esp_err_t modbus_set_relay(int index, bool state) {
    // index: 0-15 (0-7 for Relay 1, 8-15 for Relay 2)
    if (index < 0 || index > 15) return ESP_ERR_INVALID_ARG;
    
    // Determine CID
    int cid = (index < 8) ? CID_RELAY_1 : CID_RELAY_2;
    char * param_key = (index < 8) ? "Relay 1-8" : "Relay 9-16";
    int local_bit = index % 8;
    
    // We need to read current state first to preserve other bits?
    // sys_modbus_data is updated by poll task. We use that.
    // CAUTION: Race condition possible if poll task updates while we calculate.
    // For this simple HMI, it should be acceptable.
    
    uint8_t current_byte = 0;
    // Reconstruction from existing bool array
    int start_offset = (index < 8) ? 0 : 8;
    for(int i=0; i<8; i++) {
        if(sys_modbus_data.relays[start_offset + i]) current_byte |= (1 << i);
    }
    
    if (state) current_byte |= (1 << local_bit);
    else current_byte &= ~(1 << local_bit);
    
    // Write
    uint8_t type = 0;
    return mbc_master_set_parameter(cid, param_key, (uint8_t*)&current_byte, &type);
}
