#ifndef WATERHEATEREMULATOR_H_INCLUDED
#define WATERHEATEREMULATOR_H_INCLUDED

#include <string>
#include <map>
#include "DistributedEnergyResource.h"
#include "tsu.h"

class WaterHeaterEmulator: public DistributedEnergyResource {
public:
	WaterHeaterEmulator (tsu::config_map &config);
	virtual ~WaterHeaterEmulator ();

public:
	virtual void Loop (float delta_time);
	void SetBypassImportWatts (float power);
	void SetBypassImportPower (float power);
	void SetDeltaEnergy (float energy);
	unsigned int GetBypassImportWatts ();
	float GetBypassImportPower ();
	float GetDeltaEnergy ();

public:
	unsigned int mains_temp_;
	unsigned int temp_setpoint_;
	unsigned int thermal_ramp_;

public:
	virtual void ImportPower (float delta_time);
	void IdleLoss (float delta_time);
	void Usage ();
	void UsageLoss (float delta_time);
	void Log ();
	void Display ();
	unsigned int last_utc_;
	unsigned int log_inc_;
	std::string log_path_;
	float delta_energy_;

public:
	tsu::string_matrix schedule_;
	float bypass_import_power_;
	unsigned int bypass_import_watts_;

};

#endif // WATERHEATEREMULATOR_H_INCLUDED