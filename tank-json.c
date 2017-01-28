#define SERVER_PORT 6789

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <jsonrpc-c.h>
#include <grovepi.h>

#define MAX_THROTTLE_VALUE 32768
#define MAX_ROTATION_VALUE 32768
#define MAX_TURRET_VALUE 32768

#define PANIC_BUTTON 1

#define DIRECTION_FORWARD 1
#define DIRECTION_BACKWARD 0

#define PWM_MIN 48
#define PWM_MAX 255

#define GROVEPI_PORT_THROTTLE_LEFT 6
#define GROVEPI_PORT_THROTTLE_RIGHT 5

#define GROVEPI_PORT_DIRECTION_LEFT 7
#define GROVEPI_PORT_DIRECTION_RIGHT 8

#define GROVEPI_PORT_TURRET 3
#define GROVEPI_PORT_TURRET_DIR_1 2
#define GROVEPI_PORT_TURRET_DIR_2 4

#define GROVEPI_PORT_BUZZER 15 // Port A1

#define PORT 6789


struct jrpc_server tank_server;


void move_tank(double move_l, double move_r, double move_t) {
	double pwm_move_l, pwm_move_r, pwm_move_t;
	short dir_l, dir_r, dir_t;
	
	
	pwm_move_l = (PWM_MAX - PWM_MIN) * move_l + PWM_MIN;
	pwm_move_r = (PWM_MAX - PWM_MIN) * move_r + PWM_MIN;
	pwm_move_t = (PWM_MAX - PWM_MIN) * move_t + PWM_MIN;

	if (pwm_move_l < 0) pwm_move_l = -pwm_move_l;
	if (pwm_move_r < 0) pwm_move_r = -pwm_move_r;
	if (pwm_move_t < 0) pwm_move_t = -pwm_move_t;
	
	dir_l = move_l < 0 ? 1 : 0;
	dir_r = move_r < 0 ? 1 : 0;
	dir_t = move_t < 0 ? 1 : 0;
	
	printf("pwm: %i dir: %i\n", pwm_move_l, dir_l);
	printf("pwm: %i dir: %i\n", pwm_move_r, dir_r);
	printf("pwm: %i dir: %i\n", pwm_move_t, dir_t);
	
	digitalWrite(GROVEPI_PORT_DIRECTION_LEFT, dir_l);
	digitalWrite(GROVEPI_PORT_DIRECTION_RIGHT, dir_r);
	analogWrite(GROVEPI_PORT_THROTTLE_LEFT, pwm_move_l);
	analogWrite(GROVEPI_PORT_THROTTLE_RIGHT, pwm_move_r);

}

void beep(int rep) {
	int i = 0;
	for (i = 0; i < rep; i++) {
		#ifdef __arm__
		digitalWrite(GROVEPI_PORT_BUZZER, 1);
		usleep(5000);
		digitalWrite(GROVEPI_PORT_BUZZER, 0);
		usleep(150000);
		#endif
	}
}

void full_stop() {
#ifdef __arm__
	analogWrite(GROVEPI_PORT_THROTTLE_LEFT, 0);
	analogWrite(GROVEPI_PORT_THROTTLE_RIGHT, 0);
	analogWrite(GROVEPI_PORT_TURRET, 0);
	digitalWrite(GROVEPI_PORT_TURRET_DIR_1, 1);
	digitalWrite(GROVEPI_PORT_TURRET_DIR_2, 1);
#endif
	beep(5);
	// exit(0);
}

cJSON * move(jrpc_context * ctx, cJSON * pars, cJSON *id) {
	double move_l = cJSON_GetObjectItem(pars, "move_l")->valuedouble;
	double move_r = cJSON_GetObjectItem(pars, "move_r")->valuedouble;
	double move_t = cJSON_GetObjectItem(pars, "move_t")->valuedouble;
	
	move_tank(move_l, move_r, move_t);	
	
	return cJSON_CreateString("Yes sir!");
}

cJSON * stop_n_quit(jrpc_context * ctx, cJSON * pars, cJSON *id) {
	full_stop();
	jrpc_server_stop(&tank_server);
	return cJSON_CreateString("I quit!");
}

cJSON * test(jrpc_context * ctx, cJSON * pars, cJSON *id) {
	beep(3);
	printf("JSON String pars : %s\n", cJSON_Print(pars));
	printf("JSON String id : %s\n", cJSON_Print(id));
	return cJSON_CreateString("I'm here, what do you want.");
}


void init_grove_pi() {
#ifdef __arm__
	init();
	pinMode(GROVEPI_PORT_THROTTLE_LEFT,1);
	pinMode(GROVEPI_PORT_DIRECTION_LEFT,1);
	pinMode(GROVEPI_PORT_THROTTLE_RIGHT,1);
	pinMode(GROVEPI_PORT_DIRECTION_RIGHT,1);

	pinMode(GROVEPI_PORT_BUZZER, 1);

	pinMode(GROVEPI_PORT_TURRET,1);
	pinMode(GROVEPI_PORT_TURRET_DIR_1,1);
	pinMode(GROVEPI_PORT_TURRET_DIR_2,1);

	analogWrite(GROVEPI_PORT_THROTTLE_LEFT, 0);
	analogWrite(GROVEPI_PORT_THROTTLE_RIGHT, 0);
	analogWrite(GROVEPI_PORT_TURRET, 0);
	digitalWrite(GROVEPI_PORT_TURRET_DIR_1, 1);
	digitalWrite(GROVEPI_PORT_TURRET_DIR_2, 1);
#endif
}


int main(void) {
	init_grove_pi();
	
	jrpc_server_init(&tank_server, SERVER_PORT);
	jrpc_register_procedure(&tank_server, move, "move", NULL );
	jrpc_register_procedure(&tank_server, stop_n_quit, "stopnquit", NULL );
	jrpc_register_procedure(&tank_server, test, "test", NULL );
	jrpc_server_run(&tank_server);
	jrpc_server_destroy(&tank_server);
	
	printf("Peace at last!\n");
	
	return 0;
}
