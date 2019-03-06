#ifndef WATERHEATEREMULATOR_H_INCLUDED
#define WATERHEATEREMULATOR_H_INCLUDED

#include <string>
#include <map>
#include "DistributedEnergyResource.h"
#include "tsu.h"

class WaterHeaterEmulator: public DistributedEnergyResource {
public:
	WaterHeaterEmulator (tsu::config_map &config, unsigned int ID);
	virtual ~WaterHeaterEmulator ();

public:
	void Loop (float delta_time);
	void SetBypassImportWatts (float power);
	void SetBypassImportPower (float power);
	void SetNormalImportPower (float power);
	void SetDeltaEnergy (float energy);
	void SetImportEnergyFloat (float energy);
	unsigned int GetBypassImportWatts ();
	float GetBypassImportPower ();
	float GetNormalImportPower ();
	float GetDeltaEnergy ();
	float GetImportEnergyFloat ();

private:
	void ImportPower (float delta_time);
	void IdleLoss (float delta_time);
	void Usage ();
	void UsageLoss (float delta_time);
	void Log ();
	void Display ();
	unsigned int last_utc_;
	unsigned int log_inc_;
	std::string log_path_;
	float delta_energy_;

private:
	tsu::string_matrix schedule_;
	float bypass_import_power_;
	float normal_import_power_;
	unsigned int bypass_import_watts_;
	unsigned int mains_temp_;
	unsigned int temp_setpoint_;
	unsigned int thermal_ramp_;
	unsigned int ID_;
	float import_energy_float_;
};

#endif // WATERHEATEREMULATOR_H_INCLUDED