#ifndef ARGV_H
#define ARGV_H

void SetupArgv (int argc, const char** argv);

const char **GetArgv();
int GetArgc();

/// Returns true if the commandline contains a -name.
bool HasARGV (const std::string& name);

bool IsBatchmode ();
bool IsHumanControllingUs ();

void SetIsBatchmode (bool value);

/// Returns a list of values for the argument
std::vector<std::string> GetValuesForARGV (const std::string& name);

std::vector<std::string> GetAllArguments();
std::string GetFirstValueForARGV (const std::string& name);

void SetRelaunchApplicationArguments (const std::vector<std::string>& args);
std::vector<std::string> GetRelaunchApplicationArguments ();

void PrintARGV ();

void CheckBatchModeErrorString (const std::string& error);

#endif
