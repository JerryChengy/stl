
#include "ConstantString.h"
#include "UnityPrefix.h"
#include "MemoryManager.h"
#include "MemoryMacros.h"
#include "ConstantStringManager.h"
#include <iostream>
using namespace std;


int main()
{
	UInt32     m_MemLabel = 0;
	MemLabelIdentifier id = (MemLabelIdentifier)m_MemLabel;
	MemLabelId label(id, NULL);

	MemoryManager::StaticInitialize();
	ConstantStringManager::StaticInitialize();
	ConstantString cs;
	cs.assign("aaa", label);
	std::cout<<cs.c_str();
	
	//testlabelid iii;
	//GetMemoryManager ().TestFunc(20, 16, iii);
	//GetMemoryManager ().TestFunc1(20, 16, label);
//	char* aaa = (char*)UNITY_MALLOC (label, 100);
	//GetMemoryManager().ThreadCleanup();
	system("pause");
	return 0;
}