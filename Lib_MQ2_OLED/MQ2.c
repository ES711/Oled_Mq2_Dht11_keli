
#include "MQ2.h"
#include "adc.h"
#include "math.h"
float readLPG(){
	return  MQGetGasPercentage(MQRead()/Ro,GAS_LPG);
}

float readCO(){
	return MQGetGasPercentage(MQRead()/Ro,GAS_CO);
}

float readSmoke(){
	return  MQGetGasPercentage(MQRead()/Ro,GAS_SMOKE);
}

float MQResistanceCalculation(int raw_adc) {
	return (((float)RL_VALUE*(4095-raw_adc)/raw_adc));
}

float MQCalibration() {
	HAL_ADC_Start(&hadc1);
	HAL_ADC_PollForConversion(&hadc1,1500);
  static float val=0;
  for (int i=0;i<CALIBARAION_SAMPLE_TIMES;i++) {  		//take multiple samples
    val += MQResistanceCalculation(HAL_ADC_GetValue(&hadc1));
		
    HAL_ADC_PollForConversion(&hadc1,CALIBRATION_SAMPLE_INTERVAL);
  }
  val = val/CALIBARAION_SAMPLE_TIMES;                   //calculate the average value
 
  val = val/RO_CLEAN_AIR_FACTOR;                        //divided by RO_CLEAN_AIR_FACTOR yields the Ro 
                                                        //according to the chart in the datasheet 
  return val; 
}
float MQRead() {
  
  float rs=0;
	for (int i=0;i<READ_SAMPLE_TIMES;i++) {
	
		rs += MQResistanceCalculation(HAL_ADC_GetValue(&hadc1));
		
		HAL_ADC_PollForConversion(&hadc1,READ_SAMPLE_INTERVAL);
	}
  rs = rs/READ_SAMPLE_TIMES;
	
  return rs;  
}
int MQGetGasPercentage(float rs_ro_ratio, int gas_id) {
  if ( gas_id == GAS_LPG ) {
     return MQGetPercentage(rs_ro_ratio,LPGCurve);
		
  } else if ( gas_id == GAS_CO ) {
     return MQGetPercentage(rs_ro_ratio,COCurve);
		
  } else if ( gas_id == GAS_SMOKE ) {
     return MQGetPercentage(rs_ro_ratio,SmokeCurve);
		
  }    
  return 0;
}
int MQGetPercentage(float rs_ro_ratio, float *pcurve) {
  return (pow(10,(((log(rs_ro_ratio)-pcurve[1])/pcurve[2]) + pcurve[0])));
}
