#ifndef PORT_SAVE_TRANSFER_H
#define PORT_SAVE_TRANSFER_H
#ifdef __cplusplus
extern "C" {
#endif
int portSaveTransferStartup(int argc, char **argv);
void portSaveTransferRequest(int exporting);
#ifdef __cplusplus
}
#endif
#endif
