#include "subWrapper.h"

#include "../libnoisered/subtraction_manager.h"
#include <iostream>
#include "audiomanager.h"
#include <QDebug>

extern SubtractionManager* s_data;
extern AudioManager* am;
// Améliorer l'allocation : si on est plus grand, juste augmenter, si on est plus petit, ne rien faire
#ifdef __cplusplus
extern "C"
{
#endif
void computeSS(signed short int* buffer, int len)
{
	s_data->readBuffer(buffer, len);
	s_data->execute();
	s_data->writeBuffer(buffer);

	am->writeAudio(buffer, len);
}

void resetSS()
{
	s_data->onDataUpdate();
}

#ifdef __cplusplus
}
#endif
