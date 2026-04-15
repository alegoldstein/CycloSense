//gps.h
//pin + variable definitions and function declarations




//mode options
#define MODE_GLL (1 << 0)
#define MODE_RMC (1 << 1)
#define MODE_VTG (1 << 2)
#define MODE_GGA (1 << 3)
#define MODE_GSA (1 << 4)
#define MODE_GSV (1 << 5)

enum heading {
    NORTH, EAST, SOUTH, WEST;
};

typedef struct {
    double longitude;
    double latitude;
    uint32_t time;
    uint32_t date;
    uint32_t gps_speed;
    uint32_t num_satellites;
    enum heading direction;
} GPS_data ;

////////////////////////////////////////////////////
// set_mode: set one of GLL,RMC,VTG,GGA,GSA,GSV
// doesnt reset if other modes are also active, can reset with mod = 0
// frequency can be lowered by increasing freq variable, 5 = one read every 5 fixes
////////////////////////////////////////////////////
//ex: $PMTK314,1,1,1,1,1,5,0,0,0,0,0,0,0,0,0,0,0,0,0*2C<CR><LF>
void set_mode(uint8_t mod, uint8_t freq);



////////////////////////////////////////////////////
// parse_data: split up data into relevant variables
////////////////////////////////////////////////////
struct GPS_data parse_data(char* received_data);



