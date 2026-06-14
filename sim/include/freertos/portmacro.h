#ifndef DESKCLOCK_SIM_PORTMACRO_H
#define DESKCLOCK_SIM_PORTMACRO_H

using portMUX_TYPE = int;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(mux) do { (void)(mux); } while (0)
#define portEXIT_CRITICAL(mux) do { (void)(mux); } while (0)

#endif /* DESKCLOCK_SIM_PORTMACRO_H */
