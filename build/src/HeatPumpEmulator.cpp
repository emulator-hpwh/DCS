#include "include/HeatPumpEmulator.h"
#include "include/WaterHeaterEmulator.h"
#include <string>
#include <random>
#include <algorithm>
#include <vector>
#include <time.h>
#include <map>
#include "include/DistributedEnergyResource.h"
#include "include/tsu.h"
#include <iostream>
#include "include/logger.h"

HeatPumpEmulator::HeatPumpEmulator (tsu::config_map &config) :
	WaterHeaterEmulator (config) {
	SetBypassImportWatts (0);
};

HeatPumpEmulator::~HeatPumpEmulator () {};

float HeatPumpEmulator::HeatPumpPower (float import_energy) {
	float power;
	power = 454 - import_energy*0.0414;
	return power;
}

void HeatPumpEmulator::Loop (float delta_time) {
	float import_energy = GetImportEnergy ();
	unsigned int import_watts = GetImportWatts ();
	unsigned int bypass_watts = GetBypassImportWatts ();
	unsigned int element_power = 4500;
	
	if (import_energy > 1875) {
		SetBypassImportWatts (element_power + HeatPumpPower(import_energy));
	} else if (import_energy > 1800 && bypass_watts > element_power) {
		SetBypassImportWatts (element_power + HeatPumpPower(import_energy));
	} else if (import_energy > 1575) {
		SetBypassImportWatts (HeatPumpPower(import_energy));
	} else if (import_energy > 900 && bypass_watts > 0) {
		SetBypassImportWatts (HeatPumpPower(import_energy));
	} else {
		SetBypassImportWatts (0);
	}

	if ((import_watts > 0 || GetBypassImportWatts () > 0) && import_energy > 50) {
		HeatPumpEmulator::ImportPower (delta_time);
	}else if ((import_watts > 0 && GetImportPower () > 0) && import_energy > 0) {
		HeatPumpEmulator::ImportPower (delta_time);
	} else {
		IdleLoss (delta_time);
	}
	Usage ();
	UsageLoss (delta_time);
	Log ();
};

void HeatPumpEmulator::ImportPower (float delta_time) {
	float seconds = delta_time / 1000;
	float hours = seconds / (60*60);
	float import_energy = GetImportEnergy ();
	unsigned int import_watts = GetImportWatts();
	float delta_energy;
	unsigned int bypass_watts = GetBypassImportWatts ();
	unsigned int element_power = 4500;
	float heat_pump_output = 987.606;
	
	SetBypassImportPower (bypass_watts);

	if (import_watts == 0) {
		SetImportPower (0);
		//delta_energy = GetBypassImportPower () * hours;
		/*
		DEBUGGING: ADDING CoP FACTOR TO HEAT PUMP INPUT (ALSO WRONG)
		if (delta_energy < 4500) {
			delta_energy = delta_energy * 2.36;
		} else {
			delta_energy = 4500 + (delta_energy - 4500)*2.36;
		}
		*/

		//DEBUGGING: ASSUMING CONSTANT RATE OF HEATING FROM HP
		if (GetBypassImportPower () < element_power) {
			delta_energy = heat_pump_output * hours;
		} else {
			delta_energy = (element_power + heat_pump_output) * hours;
		}


		SetImportEnergy (import_energy - delta_energy);
	} else {
		if (import_energy > 1575) {
			HeatPumpEmulator::SetImportPower (element_power
				+ HeatPumpEmulator::HeatPumpPower(import_energy));
		} else if (import_energy < 1575 && import_energy > 50) {
			HeatPumpEmulator::SetImportPower (HeatPumpEmulator::HeatPumpPower (import_energy));
		} else if (import_energy < 50 && GetImportPower () > 0) {
			HeatPumpEmulator::SetImportPower (HeatPumpEmulator::HeatPumpPower (import_energy));
		} else if (import_energy < 50 && GetImportPower () == 0) {
			HeatPumpEmulator::SetImportPower (0);
		}
		delta_energy = GetImportPower () * hours;
		
		/*
		DEBUGGING: ADDING CoP FACTOR TO HEAT PUMP INPUT
		if (delta_energy < 4500) {
			delta_energy = delta_energy * 2.36;
		} else {
			delta_energy = 4500 + (delta_energy - 4500)*2.36;
		}
		*/

		//DEBUGGING: ASSUMING CONSTANT RATE OF HEATING FROM HP
		if (GetBypassImportPower () < element_power) {
			delta_energy = heat_pump_output * hours;
		} else {
			delta_energy = (element_power + heat_pump_output) * hours;
		}

		SetImportEnergy (import_energy - delta_energy);
		if (GetImportEnergy () == 0) {
			SetImportPower (0);
		}
	}
};

