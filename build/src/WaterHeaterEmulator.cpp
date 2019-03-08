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

WaterHeaterEmulator::WaterHeaterEmulator (tsu::config_map &config, unsigned int ID) : 
	mains_temp_(stoul(config["EWH"]["mains_temp"])), 
	temp_setpoint_(stoul(config["EWH"]["temp_setpoint"])),
	thermal_ramp_(stoul(config["EWH"]["thermal_ramp"])),
	ID_(ID) {

	//Determine household size
	std::srand(time(NULL));
	std::random_device rd;
	std::mt19937 gen(rd());
	//std::binomial_distribution<> d(4,0.5);
	//int size;
	//size = d(gen) +1;
	
	float rn = float(rand())/RAND_MAX;
	int size;
	if (rn <= 0.133) {
		size = 1;
	} else if (rn <= 0.396) {
		size = 2;
	} else if (rn <= 0.792) {
		size = 3;
	} else if (rn <= 0.957) {
		size = 4;
	} else {
		size = 5;
	}

	//Read schedule and shuffle volumes column
	if (size == 1) {
		schedule_ = tsu::FileToMatrix("../data/1bed.csv", ',' ,2);
	} else if (size == 2) {
		schedule_ = tsu::FileToMatrix("../data/2bed.csv", ',' ,2);
	} else if (size == 3) {
		schedule_ = tsu::FileToMatrix("../data/3bed.csv", ',' ,2);
	} else if (size == 4) {
		schedule_ = tsu::FileToMatrix("../data/4bed.csv", ',' ,2);
	} else if (size == 5) {
		schedule_ = tsu::FileToMatrix("../data/5bed.csv", ',' ,2);
	}

	
	//Normal distribution of event times and volumes
	time_t utc = time(0);
	tm now = *localtime(&utc);
	std::string schedule_formatted_time;
	time_t event_utc;
	tm event_time;
	char event_formatted_time [80];
	float mean_time;
	float time_std_dev = 60*30; //half hour standard deviation for times
	float mean_event_vol;
	float event_vol_std_dev;
	float event_vol;

	for (unsigned int i=0; i<schedule_.size(); i++) {
		schedule_formatted_time = schedule_[i][0];
		strptime(schedule_formatted_time.c_str(), "%T", &now);
		mean_time = mktime(&now);
		std::normal_distribution<double> time_distribution(mean_time,time_std_dev);
		event_utc = int(time_distribution(gen));
		event_time = *localtime(&event_utc);
		strftime(event_formatted_time,80,"%T",&event_time);
		schedule_[i].at(0) = event_formatted_time;
		
		mean_event_vol = stof(schedule_[i][1]);
		event_vol_std_dev = mean_event_vol * 0.3;
		std::normal_distribution<double> event_vol_distribution(mean_event_vol,event_vol_std_dev);
		event_vol = -1;
		while (event_vol < 0) {
			event_vol = event_vol_distribution(gen);
		}
		schedule_[i].at(1) = std::to_string(event_vol);
	}			

	//Setting member properties for WHs *I will probably remove/replace this*
	SetRatedImportPower(stoul(config["EWH"]["rated_import_power"]));
	SetRatedImportEnergy(3630);
	SetImportRamp(4500);
	SetExportRamp(0);
	last_utc_ = 0;
	log_inc_ = stoul(config["DER"]["log_inc"]);
	log_path_ = config["DER"]["log_path"];
	SetIdleLosses (stoul(config["EWH"]["idle_losses"]));
	WaterHeaterEmulator::SetBypassImportWatts (0);
	WaterHeaterEmulator::SetBypassImportPower (0);
	WaterHeaterEmulator::SetNormalImportPower (0);
	SetImportPower (0);
	SetImportWatts (0);
	SetDeltaEnergy(0);

	//Randomize starting energy state, with middle value as mean
	float mean = 0.5;
	float std_dev = 0.3;
	float ratio;
	std::normal_distribution<double> distribution(mean,std_dev);
	ratio = 2;
	while (ratio > 1 || ratio < 0) {
		ratio = distribution(gen);
	}
	SetImportEnergy(ratio*GetRatedImportEnergy ());
	WaterHeaterEmulator::SetImportEnergyFloat (GetImportEnergy ());

	std::cout
		<< "Emulator Initialized:\n"
		<< "Household size:\t" << size << " bedrooms\n"
		<< "Import Energy:\t" << GetImportEnergy () << " watt-hours\n"
		<< "Usage Schedule:\n";
	for (unsigned int i=0; i<schedule_.size(); i++) {
		std::cout
			<< schedule_[i][0] << '\t'
			<< schedule_[i][1] << '\n';
	}


};

WaterHeaterEmulator::~WaterHeaterEmulator () {};

void WaterHeaterEmulator::Loop (float delta_time) {
	float import_energy = WaterHeaterEmulator::GetImportEnergyFloat ();
	if (import_energy > 2025) {
		WaterHeaterEmulator::SetBypassImportWatts (GetRatedImportPower ());
	} else if (import_energy < 1725 && GetBypassImportPower () != 0){
		WaterHeaterEmulator::SetBypassImportWatts (0);
		WaterHeaterEmulator::SetBypassImportPower (0);
	}

	if ((GetImportWatts () > 0 || WaterHeaterEmulator::GetBypassImportWatts () > 0) && import_energy > 300) {
		WaterHeaterEmulator::ImportPower (delta_time);
	} else if (GetImportWatts () > 0 && GetImportPower () > 0 && import_energy > 0) {
		WaterHeaterEmulator::ImportPower (delta_time);
	} else {
		WaterHeaterEmulator::IdleLoss (delta_time);
	}
	WaterHeaterEmulator::Usage ();
	WaterHeaterEmulator::UsageLoss (delta_time);
	WaterHeaterEmulator::Log ();
};

void WaterHeaterEmulator::ImportPower (float delta_time) {
	float seconds = delta_time / 1000;
	float hours = seconds / (60*60);
	float import_energy = WaterHeaterEmulator::GetImportEnergyFloat ();
	float import_power = GetImportPower ();
	float import_watts = GetImportWatts ();
	float delta_energy;

	if (WaterHeaterEmulator::GetBypassImportWatts () > 0) {
		if (WaterHeaterEmulator::GetBypassImportPower () < WaterHeaterEmulator::GetBypassImportWatts ()) {
			WaterHeaterEmulator::SetBypassImportPower (WaterHeaterEmulator::GetBypassImportWatts ());
		}
		delta_energy = WaterHeaterEmulator::GetBypassImportPower () * hours;
		WaterHeaterEmulator::SetImportEnergyFloat (import_energy - delta_energy);
		SetImportEnergy (import_energy - delta_energy);
	} else {
		if (import_power < import_watts && import_energy > 300) {
			SetImportPower (GetRatedImportPower ());
		}
		delta_energy = GetImportPower () * hours;
		WaterHeaterEmulator::SetImportEnergyFloat (import_energy - delta_energy);
		SetImportEnergy (import_energy - delta_energy);
	}
	if (WaterHeaterEmulator::GetBypassImportPower () > 0) {
		SetImportPower (GetRatedImportPower ());
	}
};

void WaterHeaterEmulator::IdleLoss (float delta_time) {
	float seconds = delta_time / 1000;
	float hours = seconds / (60*60);
	float import_energy = WaterHeaterEmulator::GetImportEnergyFloat ();
	float loss_factor = 1 - import_energy/GetRatedImportEnergy ();
	float energy_loss = GetIdleLosses () * hours * loss_factor;
	SetImportPower (0);
	//WaterHeaterEmulator::SetNormalImportPower (0);
	WaterHeaterEmulator::SetImportEnergyFloat (import_energy + energy_loss);
	SetImportEnergy(import_energy + energy_loss);
};

void WaterHeaterEmulator::Usage () {
	time_t now = time(0);
	char time_formatted[100];
	tm now_local = *localtime(&now);
	strftime(time_formatted, sizeof(time_formatted), "%T", &now_local);
	float tank_temp;

	float import_energy;
	float delta_energy;
	float old_delta_energy = WaterHeaterEmulator::GetDeltaEnergy ();
	for (unsigned int i=0; i<schedule_.size(); i++) {
		if (time_formatted == schedule_[i][0]) {
			import_energy = WaterHeaterEmulator::GetImportEnergyFloat ();
			tank_temp = temp_setpoint_ - import_energy/100;
			delta_energy = stof(schedule_[i][1])*(tank_temp - mains_temp_)*2.44;
			//SetImportEnergy(import_energy + delta_energy);
			WaterHeaterEmulator::SetDeltaEnergy (old_delta_energy + delta_energy);
		}
	}
};

void WaterHeaterEmulator::UsageLoss (float delta_time) {
	float seconds = delta_time / 1000;
	float hours = seconds / (60*60);
	float import_energy = WaterHeaterEmulator::GetImportEnergyFloat ();
	float delta_energy = WaterHeaterEmulator::GetDeltaEnergy ();

	if (delta_energy > thermal_ramp_ * hours) {
		WaterHeaterEmulator::SetImportEnergyFloat(import_energy + thermal_ramp_ * hours);
		SetImportEnergy (import_energy + thermal_ramp_ * hours);
		WaterHeaterEmulator::SetDeltaEnergy (delta_energy - thermal_ramp_ * hours);
	} else {
		WaterHeaterEmulator::SetImportEnergyFloat (import_energy + delta_energy);
		SetImportEnergy (import_energy + delta_energy);
		WaterHeaterEmulator::SetDeltaEnergy (0);
	}
};

void WaterHeaterEmulator::Log () {
	unsigned int utc = time (0);
	if ((utc % log_inc_ == 0) && (last_utc_ != utc)) {
		Logger ("Emulator_Data", log_path_)
			<< "Emulator ID#: " << ID_ << "\t"
			<< GetExportWatts () << "\t"
			<< GetExportPower () << "\t"
			<< GetExportEnergy () << "\t"
			<< GetImportWatts () << "\t"
			<< GetImportPower () << "\t"
			<< GetImportEnergy () << "\t"
			<< GetBypassImportPower () << "\t"
			<< WaterHeaterEmulator::GetImportEnergyFloat () << "\t";
		last_utc_ = utc;
	}
};

void WaterHeaterEmulator::Display () {
	std::cout
		<< "Import Power:\t" << GetImportPower () << "\twatts\n"
		<< "Bypass Power:\t" << WaterHeaterEmulator::GetBypassImportPower () << "\twatts\n"
		<< "Import Control:\t" << GetImportWatts () << "\twatts\n"
		<< "Import Energy:\t" << WaterHeaterEmulator::GetImportEnergyFloat () << "\twatt-hours\n"
		<< std::endl;
};

float WaterHeaterEmulator::GetBypassImportPower () {
	return bypass_import_power_;
};

unsigned int WaterHeaterEmulator::GetBypassImportWatts () {
	return bypass_import_watts_;
};

void WaterHeaterEmulator::SetBypassImportPower (float power) {
	if (power > WaterHeaterEmulator::GetBypassImportWatts ()) {
		bypass_import_power_ = GetBypassImportWatts ();
	} else if (power <= 0) {
		bypass_import_power_ = 0;
	} else {
		bypass_import_power_ = power;
	}
};

void WaterHeaterEmulator::SetBypassImportWatts (float watts) {
	if (watts > GetRatedImportPower ()) {
		bypass_import_watts_ = GetRatedImportPower ();
	} else {
		bypass_import_watts_ = watts;
	}
};

void WaterHeaterEmulator::SetDeltaEnergy (float energy) {
	delta_energy_ = energy;
};

float WaterHeaterEmulator::GetDeltaEnergy () {
	return delta_energy_;
};

float WaterHeaterEmulator::GetNormalImportPower () {
	return normal_import_power_;
};

void WaterHeaterEmulator::SetNormalImportPower (float power) {
	normal_import_power_ = power;
};

float WaterHeaterEmulator::GetImportEnergyFloat () {
	return import_energy_float_;
};

void WaterHeaterEmulator::SetImportEnergyFloat (float energy) {
	if (energy > GetRatedImportEnergy ()) {
		import_energy_float_ = GetRatedImportEnergy ();
	} else if (energy <= 0) {
		import_energy_float_ = 0;
	} else {
		import_energy_float_ = energy;
	}
};
