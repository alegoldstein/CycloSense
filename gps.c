//gps.c
//get longitude/langitude coordinates, time, and heading from gps sensor

//will get to RTOS when we get there, but likely use circular buffer to fill incoming data before parsing in main

void set_mode(uint8_t mod, uint8_t freq){
    uint8_t GLL = (mod & (1 << 0)) ? freq : 0;
    uint8_t RMC = (mod & (1 << 1)) ? freq : 0;
    uint8_t VTG = (mod & (1 << 2)) ? freq : 0;
    uint8_t GGA = (mod & (1 << 3)) ? freq : 0;
    uint8_t GSA = (mod & (1 << 4)) ? freq : 0;
    uint8_t GSV = (mod & (1 << 5)) ? freq : 0;

    // format PMTK string
    // $PMTK314,<GLL>,<RMC>,<VTG>,<GGA>,<GSA>,<GSV>,0,...*CS
    char cmd[128];
    sprintf(cmd,
        "$PMTK314,%d,%d,%d,%d,%d,%d,0,0,0,0,0,0,0,0,0,0,0,0,0,0",
        GLL, RMC, VTG, GGA, GSA, GSV
    );

    // TODO: compute checksum and append *XX\r\n

    uart_write(cmd);
}


////////////////////////////////////////////////////
// parse_data: split up data into relevant variables
////////////////////////////////////////////////////
struct GPS_data parse_data(char* received_data);