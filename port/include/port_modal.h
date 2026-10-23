/* Intentional native waits must not be mistaken for a stalled game loop. */
#ifndef PORT_MODAL_H
#define PORT_MODAL_H
#ifdef __cplusplus
extern "C" {
#endif
void portModalBegin(void);
void portModalEnd(void);
int portModalActive(void);
#ifdef __cplusplus
}
class PortModalScope {
public:
    PortModalScope() { portModalBegin(); }
    ~PortModalScope() { portModalEnd(); }
    PortModalScope(const PortModalScope &)=delete;
    PortModalScope &operator=(const PortModalScope &)=delete;
};
#endif
#endif
