#include <main/mt_uievents.h>

static arch_uievent_ops_t jiwis8xx_uievent_ops;

arch_uievent_ops_t *arch_uievent_init(void)
{
	return &jiwis8xx_uievent_ops;
}
