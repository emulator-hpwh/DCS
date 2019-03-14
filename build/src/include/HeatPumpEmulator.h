#ifndef HEATPUMPEMULATOR_H_INCLUDED
#define HEATPUMPEMULATOR_H_INCLUDED

#include <string>
#include <map>
#include "WaterHeaterEmulator.h"
#include "DistributedEnergyResource.h"
#include "tsu.h"

class HeatPumpEmulator: public WaterHeaterEmulator {
public:
	HeatPumpEmulator (tsu::config_map &config);
	virtual ~HeatPumpEmulator ();

	void Loop (float delta_time);
	void ImportPower (float delta_time);

private:
	float HeatPumpPower (float import_energy);
};

#endif // HEATPUMPEMULATOR_H_INCLUDED