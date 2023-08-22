#ifndef MQ2_h
#define MQ2_h
/* USER CODE BEGIN PD */
#define         RL_VALUE                     (5)     //5K
#define         RO_CLEAN_AIR_FACTOR          (9.83f)  //
                                           
#define         CALIBRATION_SAMPLE_INTERVAL   (15)  //recommend 15 //define the time interal(in milisecond) between each samples in the cablibration phase                                           
#define         CALIBARAION_SAMPLE_TIMES      (50)  //recommend 50//define how many samples you are going to take in the calibration 
                                                   
#define         READ_SAMPLE_INTERVAL         (60)   //recommend 60//define the time interal(in milisecond) between each samples in READ_SAMPLE phase
#define         READ_SAMPLE_TIMES            (5)    //recommend 5 //define how many samples you are going to take in normal operation
                                                
#define         GAS_LPG                     (0)
#define         GAS_CO                      (1)
#define         GAS_SMOKE										(2)
#define 				Ro													(10) //10K



/* USER CODE END PD */
         
/* USER CODE BEGIN PV */
static float LPGCurve[3]  =  {2.3,0.21,-0.47}; 
static float COCurve[3]  =  {2.3,0.72,-0.34};   
static float SmokeCurve[3] = {2.3,0.53,-0.44};   
static int         lpg = 0;
static int         co	= 0;
static int         smoke	= 0;
/* USER CODE END PV */

/* USER CODE BEGIN Prototypes */
float MQRead(void);
float MQCalibration(void);
float MQResistanceCalculation(int raw_adc);
int  MQGetPercentage(float rs_ro_ratio, float *pcurve);
int MQGetGasPercentage(float rs_ro_ratio, int gas_id);

/* USER CODE END Prototypes */


#endif
