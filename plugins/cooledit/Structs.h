typedef struct mec
{
bool					AutoCfg;
faacEncConfiguration	EncCfg;
} MY_ENC_CFG;
// -----------------------------------------------------------------------------------------------

typedef struct mdc
{
bool					AutoCfg;
BYTE					Channels;
DWORD					BitRate;
faacDecConfiguration	DecCfg;
} MY_DEC_CFG;
