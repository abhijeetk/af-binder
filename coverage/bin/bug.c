#define AFB_BINDING_VERSION 3
#include <afb/afb-binding.h>
int afbBindingEntry(afb_api_t api)
{
	return ((int(*)())(intptr_t)0)();
}
