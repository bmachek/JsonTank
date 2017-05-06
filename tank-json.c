#define SERVER_PORT 6789

#include <stdio.h>
#include <math.h>
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
#include <time.h>
#include <pthread.h>


#define DIRECTION_FORWARD 1
#define DIRECTION_BACKWARD 0

#define PWM_MIN 0.0
#define PWM_MAX 255.0

#define GROVEPI_PORT_THROTTLE_LEFT 6
#define GROVEPI_PORT_THROTTLE_RIGHT 5

#define GROVEPI_PORT_DIRECTION_LEFT 7
#define GROVEPI_PORT_DIRECTION_RIGHT 8

#define GROVEPI_PORT_TURRET 3
#define GROVEPI_PORT_TURRET_DIR_1 2
#define GROVEPI_PORT_TURRET_DIR_2 4

#define GROVEPI_PORT_GUN 16

#define GROVEPI_PORT_BUZZER 15

struct jrpc_server tank_server;
time_t last_command;
pthread_t watchdog_thread, cannoneer_thread;
int pending_shoot = 0;
int currently_shooting = 0;


void move_tank(double move_l, double move_r, double move_t) {
	double pwm_move_l, pwm_move_r, pwm_move_t;
	short dir_l, dir_r, dir_t;
	
	dir_l = move_l < 0 ? 1 : 0;
	dir_r = move_r < 0 ? 1 : 0;
	dir_t = move_t < 0 ? 1 : 0;
	
	move_l = move_l < 0 ? -move_l : move_l;
	move_r = move_r < 0 ? -move_r : move_r;
	move_t = move_t < 0 ? -move_t : move_t;

	pwm_move_l = move_l != 0 ? ((PWM_MAX - PWM_MIN) * move_l + PWM_MIN) : 0;
	pwm_move_r = move_r != 0 ? ((PWM_MAX - PWM_MIN) * move_r + PWM_MIN) : 0;
	pwm_move_t = move_t != 0 ? ((PWM_MAX - PWM_MIN) * move_t + PWM_MIN) : 0;

	printf("pwm_l: %f dir: %i\n", pwm_move_l, dir_l);
	printf("pwm_r: %f dir: %i\n", pwm_move_r, dir_r);
	printf("pwm_t: %f dir: %i\n", pwm_move_t, dir_t);
	
	digitalWrite(GROVEPI_PORT_DIRECTION_LEFT, dir_l);
	digitalWrite(GROVEPI_PORT_DIRECTION_RIGHT, dir_r);
	digitalWrite(GROVEPI_PORT_TURRET_DIR_1, dir_t == 1 ? 0 : 1);
	digitalWrite(GROVEPI_PORT_TURRET_DIR_2, dir_t == 1 ? 1 : 0);
	analogWrite(GROVEPI_PORT_THROTTLE_LEFT, pwm_move_l);
	analogWrite(GROVEPI_PORT_THROTTLE_RIGHT, pwm_move_r);
	analogWrite(GROVEPI_PORT_TURRET, pwm_move_t);

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
	// beep(5);
	// exit(0);
}

cJSON * move(jrpc_context * ctx, cJSON * pars, cJSON *id) {
	last_command = time(NULL);
	
	double move_l = cJSON_GetObjectItem(pars, "move_l")->valuedouble;
	double move_r = cJSON_GetObjectItem(pars, "move_r")->valuedouble;
	double move_t = cJSON_GetObjectItem(pars, "move_t")->valuedouble;
	
	move_tank(move_l, move_r, move_t);	
	
	return cJSON_CreateString("Yes sir!");
}

cJSON * stop(jrpc_context * ctx, cJSON * pars, cJSON *id) {
	full_stop();
	return cJSON_CreateString("Stopping!");
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
	pinMode(GROVEPI_PORT_GUN, 1);

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

void watchdog() {
	while (1) {
		if (time(NULL) - last_command > 3) {
			// printf("Haven't heard from you in a while. Stopping!\n");
			full_stop();
			last_command = time(NULL);
		}
		sleep(1);
	}
}

cJSON * restart_cam_services() {
	system("systemctl restart turret-cam.service");
	system("systemctl restart body-cam.service");
	return cJSON_CreateString("Cam services restarted.");
}

cJSON * reboot() {
	system("reboot");
	return cJSON_CreateString("See you soon.");
}

cJSON * initiate_shoot() {
	if (currently_shooting == 0) {
		pending_shoot = 1;
		printf("Got shoot order\n");
		return cJSON_CreateString("Kaawwooom");
	} else {
		printf("We only have one gun!\n");
		return cJSON_CreateString("Calm down!");
	}
	
}


void shoot_now() {
	currently_shooting = 1;
	digitalWrite(GROVEPI_PORT_GUN, 1);
	usleep(1800);
	digitalWrite(GROVEPI_PORT_GUN, 0);
	currently_shooting = 0;
	printf("Shoot order completed.\n");
}

void cannoneer() {
	while (1) {
		if (pending_shoot == 1 && ! currently_shooting) {
			pending_shoot = 0;
			shoot_now();
		}
		usleep(200);
	}
}


int main(void) {
	init_grove_pi();
	
	last_command = time(NULL);
	
	pthread_create(&watchdog_thread, NULL, (void *) &watchdog, NULL);
	pthread_create(&cannoneer_thread, NULL, (void *) &cannoneer, NULL);
	
	jrpc_server_init(&tank_server, SERVER_PORT);
	jrpc_register_procedure(&tank_server, move, "move", NULL );
	jrpc_register_procedure(&tank_server, stop_n_quit, "stopnquit", NULL );
	jrpc_register_procedure(&tank_server, test, "test", NULL );
	jrpc_register_procedure(&tank_server, stop, "fullstop", NULL );
	jrpc_register_procedure(&tank_server, restart_cam_services, "restart_cams", NULL );
	jrpc_register_procedure(&tank_server, reboot, "reboot", NULL );
	jrpc_register_procedure(&tank_server, initiate_shoot, "shoot", NULL );
	jrpc_server_run(&tank_server);
	
	jrpc_server_destroy(&tank_server);
	
	pthread_join(watchdog_thread, NULL);
	pthread_join(cannoneer_thread, NULL);
	
	printf("Peace at last!\n");
	
	full_stop();
	
	return 0;
}
