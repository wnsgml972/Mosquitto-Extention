/*
Copyright (c) 2009-2014 Roger Light <roger@atchoo.org>

All rights reserved. This program and the accompanying materials
are made available under the terms of the Eclipse Public License v1.0
and Eclipse Distribution License v1.0 which accompany this distribution.

The Eclipse Public License is available at
http://www.eclipse.org/legal/epl-v10.html
and the Eclipse Distribution License is available at
http://www.eclipse.org/org/documents/edl-v10.php.

Contributors:
Roger Light - initial implementation and documentation.
*/

#include <assert.h>
#include <string.h>

#ifdef WIN32
#  include <winsock2.h>
#  include <aclapi.h>
#  include <io.h>
#  include <lmcons.h>
#else
#  include <sys/stat.h>
#endif


#include <mosquitto.h>
#include <memory_mosq.h>
#include <net_mosq.h>
#include <send_mosq.h>
#include <time_mosq.h>
#include <tls_mosq.h>
#include <util_mosq.h>

#ifdef WITH_BROKER
#include <mosquitto_broker.h>
#endif

#ifdef WITH_WEBSOCKETS
#include <libwebsockets.h>
#endif

/*
* hilight code
*/

#ifdef WITH_BROKER  // 전부다 broker code 임!
void hilight_init_queue(Queue *queue)
{
	queue->front = queue->rear = NULL; //front와 rear를 NULL로 설정
	queue->count = 0;//보관 개수를 0으로 설정
}

int hilight_is_empty(Queue *queue)
{
	return queue->count == 0;    //보관 개수가 0이면 빈 상태
}

void hilight_enqueue(Queue *queue, element data)
{
	Node *now = (Node *)malloc(sizeof(Node)); //노드 생성
	now->data = data;//데이터 설정
	now->next = NULL;

	if (hilight_is_empty(queue))//큐가 비어있을 때
	{
		queue->front = now;//맨 앞을 now로 설정       
	}
	else//비어있지 않을 때
	{
		queue->rear->next = now;//맨 뒤의 다음을 now로 설정
	}
	queue->rear = now;//맨 뒤를 now로 설정   
	queue->count++;//보관 개수를 1 증가
}

element hilight_dequeue(Queue *queue)
{
	element re;
	Node *now;

	re.qos = 4;

	if (hilight_is_empty(queue))//큐가 비었을 때
	{
		printf("\nEmpty Highligh Queue\n");
		return re;
	}
	now = queue->front;//맨 앞의 노드를 now에 기억
	re = now->data;//반환할 값은 now의 data로 설정
	queue->front = now->next;//맨 앞은 now의 다음 노드로 설정

	free(now);//now 소멸
	queue->count--;//보관 개수를 1 감소

	return re;
}

//sub list
void hilight_insert_node(struct mosquitto **phead, struct mosquitto *p, struct mosquitto *new_node) {

	if (*phead == NULL) {
		new_node->link = NULL;
		*phead = new_node;
	}
	else if (p == NULL) { //들어올 일 없음 항상 뒤에다 넣기 때문에
		new_node->link = *phead;
		*phead = new_node;
	}
	else {
		new_node->link = p->link;
		p->link = new_node;
	}
}
void hilight_insert_last_node(linked_list_mosquitto *_linked_list_mosquitto, struct mosquitto **phead, struct mosquitto *p, struct mosquitto *new_node) {

	if (*phead == NULL) {
		new_node->link = NULL;
		*phead = new_node;
	}
	else if (p == NULL) { //들어올 일 없음 항상 뒤에다 넣기 때문에
		new_node->link = *phead;
		*phead = new_node;
	}
	else {
		new_node->link = p->link;
		p->link = new_node;
	}

	_linked_list_mosquitto->tail = new_node; //tail은 항상! 마지막
}
void hilight_last_element_insert_subscribe(Queue *queue, struct mosquitto *context) {
	hilight_insert_last_node(&queue->rear->data._linked_list_mosquitto, &queue->rear->data._linked_list_mosquitto.head,
		queue->rear->data._linked_list_mosquitto.tail, context); //맨 뒤에다가 넣음
	queue->rear->data._linked_list_mosquitto.node_len++; //node 갯 수 증가
}
void hilight_remove_node(struct mosquitto **phead, struct mosquitto *p, struct mosquitto *removed) {
	if (p == NULL) {
		*phead = (*phead)->link;
	}
	else {
		p->link = removed->link;
	}
	//free(removed); //수정
}
void hilight_display(struct mosquitto *head) {
	struct mosquitto *p = head;
	while (p != NULL) {
		_mosquitto_log_printf(NULL, MOSQ_LOG_NOTICE, "display command state : %d", ((p->in_packet.command) & 0xF0));
		p = p->link;
	}
}
struct moquitto *hilight_find(struct mosquitto *head, struct mosquitto val) {
	struct mosquitto *p = head;
	while (p != NULL) {
		if (strcmp(p->id, val.id) == 0) {
			break;
		}
		p = p->link;
	}
	return p;
}
struct moquitto *hilight_before_find(struct mosquitto *head, struct mosquitto val) {
	struct mosquitto *p = head;
	struct mosquitto *p_before = head;
	int cnt = 0;

	while (p != NULL) {
		p = p->link;
		cnt++;
		if (cnt > 0) {
			p_before = p_before->link;
		}
	}
	return p_before;
}


//consumer
void hilight_send_data() {
	int i;
	element data;

	if (hilight_urgency_queue.count != 0) {
		_mosquitto_log_printf(NULL, MOSQ_LOG_NOTICE, "My Urgency Queue 갯 수 : %d", hilight_urgency_queue.count);
	}

	for (i = 0; i < hilight_urgency_queue.count; i++) { //my urgency
		_mosquitto_log_printf(NULL, MOSQ_LOG_NOTICE, "---------------- urgency send 시작 ----------------");
		if (!hilight_is_empty(&hilight_urgency_queue)) {
			data = hilight_dequeue(&hilight_urgency_queue);
		}

		if (hilight_db_message_write(data) == MOSQ_ERR_SUCCESS) {
			_mosquitto_free(data.topic);
			_mosquitto_free(data.payload);
		}
		_mosquitto_log_printf(NULL, MOSQ_LOG_NOTICE, "---------------- urgency send 끝 ----------------");
	}
}
int hilight_db_message_write(element data) //수정
{
	struct mosquitto *context = data._linked_list_mosquitto.head;
	int rc;
	data.qos = 0;
	if (context == NULL) {
		printf("왜 null\n");
	}
	while (context) {
		if (!context || context->sock == INVALID_SOCKET
			|| (context->state == mosq_cs_connected && !context->id)) {
			return MOSQ_ERR_INVAL;
		}

		if (context->state != mosq_cs_connected) {
			return MOSQ_ERR_SUCCESS;
		}

		rc = _mosquitto_send_publish(context, data.mid, data.topic, data.payloadlen, data.payload, data.qos, data.retain, data.retain);
		if (!rc) {
			_mosquitto_log_printf(NULL, MOSQ_LOG_NOTICE, "urgency send 성공");
		}
		else {
			_mosquitto_log_printf(NULL, MOSQ_LOG_NOTICE, "urgency send 실패");
			return rc;
		}
		context = context->link;
	}

	return MOSQ_ERR_SUCCESS;
}


//log
/*
void _hilight_system_log(struct mosquitto *mosq, const char *fmt, ...) {
	va_list va;
	int rc;

	va_start(va, fmt);
	hilight_system_log(mosq, fmt, va);
	va_end(va);
}

void hilight_system_log(struct mosquitto *mosq, const char *fmt, va_list va) {
	int len;
	char *s;

	len = strlen(fmt) + 500;
	s = _mosquitto_malloc(len * sizeof(char));
	if (!s) return;

	vsnprintf(s, len, fmt, va);
	s[len - 1] = '\0';
	mqtt3_db_messages_easy_queue(_mosquitto_get_db(), mosq, "$SYS/broker/hilight/log", 0, strlen(s) + 1, s, 0);
	_mosquitto_free(s);
}*/

#endif



// mosquitto code
int _mosquitto_packet_alloc(struct _mosquitto_packet *packet)
{
	uint8_t remaining_bytes[5], byte;
	uint32_t remaining_length;
	int i;

	assert(packet);

	remaining_length = packet->remaining_length;
	packet->payload = NULL;
	packet->remaining_count = 0;
	do {
		byte = remaining_length % 128;
		remaining_length = remaining_length / 128;
		/* If there are more digits to encode, set the top bit of this digit */
		if (remaining_length > 0) {
			byte = byte | 0x80;
		}
		remaining_bytes[packet->remaining_count] = byte;
		packet->remaining_count++;
	} while (remaining_length > 0 && packet->remaining_count < 5);
	if (packet->remaining_count == 5) return MOSQ_ERR_PAYLOAD_SIZE;
	packet->packet_length = packet->remaining_length + 1 + packet->remaining_count;
#ifdef WITH_WEBSOCKETS
	packet->payload = _mosquitto_malloc(sizeof(uint8_t)*packet->packet_length + LWS_SEND_BUFFER_PRE_PADDING + LWS_SEND_BUFFER_POST_PADDING);
#else
	packet->payload = _mosquitto_malloc(sizeof(uint8_t)*packet->packet_length);
#endif
	if (!packet->payload) return MOSQ_ERR_NOMEM;

	packet->payload[0] = packet->command;
	for (i = 0; i<packet->remaining_count; i++) {
		packet->payload[i + 1] = remaining_bytes[i];
	}
	packet->pos = 1 + packet->remaining_count;

	return MOSQ_ERR_SUCCESS;
}

#ifdef WITH_BROKER
void _mosquitto_check_keepalive(struct mosquitto_db *db, struct mosquitto *mosq)
#else
void _mosquitto_check_keepalive(struct mosquitto *mosq)
#endif
{
	time_t next_msg_out;
	time_t last_msg_in;
	time_t now = mosquitto_time();
#ifndef WITH_BROKER
	int rc;
#endif

	assert(mosq);
#if defined(WITH_BROKER) && defined(WITH_BRIDGE)
	/* Check if a lazy bridge should be timed out due to idle. */
	if (mosq->bridge && mosq->bridge->start_type == bst_lazy
		&& mosq->sock != INVALID_SOCKET
		&& now - mosq->next_msg_out - mosq->keepalive >= mosq->bridge->idle_timeout) {

		_mosquitto_log_printf(NULL, MOSQ_LOG_NOTICE, "Bridge connection %s has exceeded idle timeout, disconnecting.", mosq->id);
		_mosquitto_socket_close(db, mosq);
		return;
	}
#endif
	pthread_mutex_lock(&mosq->msgtime_mutex);
	next_msg_out = mosq->next_msg_out;
	last_msg_in = mosq->last_msg_in;
	pthread_mutex_unlock(&mosq->msgtime_mutex);
	if (mosq->keepalive && mosq->sock != INVALID_SOCKET &&
		(now >= next_msg_out || now - last_msg_in >= mosq->keepalive)) {

		if (mosq->state == mosq_cs_connected && mosq->ping_t == 0) {
			_mosquitto_send_pingreq(mosq);
			/* Reset last msg times to give the server time to send a pingresp */
			pthread_mutex_lock(&mosq->msgtime_mutex);
			mosq->last_msg_in = now;
			mosq->next_msg_out = now + mosq->keepalive;
			pthread_mutex_unlock(&mosq->msgtime_mutex);
		}
		else {
#ifdef WITH_BROKER
			if (mosq->listener) {
				mosq->listener->client_count--;
				assert(mosq->listener->client_count >= 0);
			}
			mosq->listener = NULL;
			_mosquitto_socket_close(db, mosq);
#else
			_mosquitto_socket_close(mosq);
			pthread_mutex_lock(&mosq->state_mutex);
			if (mosq->state == mosq_cs_disconnecting) {
				rc = MOSQ_ERR_SUCCESS;
			}
			else {
				rc = 1;
			}
			pthread_mutex_unlock(&mosq->state_mutex);
			pthread_mutex_lock(&mosq->callback_mutex);
			if (mosq->on_disconnect) {
				mosq->in_callback = true;
				mosq->on_disconnect(mosq, mosq->userdata, rc);
				mosq->in_callback = false;
			}
			pthread_mutex_unlock(&mosq->callback_mutex);
#endif
		}
	}
}

uint16_t _mosquitto_mid_generate(struct mosquitto *mosq)
{
	/* FIXME - this would be better with atomic increment, but this is safer
	* for now for a bug fix release.
	*
	* If this is changed to use atomic increment, callers of this function
	* will have to be aware that they may receive a 0 result, which may not be
	* used as a mid.
	*/
	uint16_t mid;
	assert(mosq);

	pthread_mutex_lock(&mosq->mid_mutex);
	mosq->last_mid++;
	if (mosq->last_mid == 0) mosq->last_mid++;
	mid = mosq->last_mid;
	pthread_mutex_unlock(&mosq->mid_mutex);

	return mid;
}

/* Check that a topic used for publishing is valid.
* Search for + or # in a topic. Return MOSQ_ERR_INVAL if found.
* Also returns MOSQ_ERR_INVAL if the topic string is too long.
* Returns MOSQ_ERR_SUCCESS if everything is fine.
*/
int mosquitto_pub_topic_check(const char *str)
{
	int len = 0;
	while (str && str[0]) {
		if (str[0] == '+' || str[0] == '#') {
			return MOSQ_ERR_INVAL;
		}
		len++;
		str = &str[1];
	}
	if (len > 65535) return MOSQ_ERR_INVAL;

	return MOSQ_ERR_SUCCESS;
}

/* Check that a topic used for subscriptions is valid.
* Search for + or # in a topic, check they aren't in invalid positions such as
* foo/#/bar, foo/+bar or foo/bar#.
* Return MOSQ_ERR_INVAL if invalid position found.
* Also returns MOSQ_ERR_INVAL if the topic string is too long.
* Returns MOSQ_ERR_SUCCESS if everything is fine.
*/
int mosquitto_sub_topic_check(const char *str)
{
	char c = '\0';
	int len = 0;
	while (str && str[0]) {
		if (str[0] == '+') {
			if ((c != '\0' && c != '/') || (str[1] != '\0' && str[1] != '/')) {
				return MOSQ_ERR_INVAL;
			}
		}
		else if (str[0] == '#') {
			if ((c != '\0' && c != '/') || str[1] != '\0') {
				return MOSQ_ERR_INVAL;
			}
		}
		len++;
		c = str[0];
		str = &str[1];
	}
	if (len > 65535) return MOSQ_ERR_INVAL;

	return MOSQ_ERR_SUCCESS;
}

/* Does a topic match a subscription? */
int mosquitto_topic_matches_sub(const char *sub, const char *topic, bool *result)
{
	int slen, tlen;
	int spos, tpos;
	bool multilevel_wildcard = false;

	if (!sub || !topic || !result) return MOSQ_ERR_INVAL;

	slen = strlen(sub);
	tlen = strlen(topic);

	if (!slen || !tlen) {
		*result = false;
		return MOSQ_ERR_INVAL;
	}

	if (slen && tlen) {
		if ((sub[0] == '$' && topic[0] != '$')
			|| (topic[0] == '$' && sub[0] != '$')) {

			*result = false;
			return MOSQ_ERR_SUCCESS;
		}
	}

	spos = 0;
	tpos = 0;

	while (spos < slen && tpos <= tlen) {
		if (sub[spos] == topic[tpos]) {
			if (tpos == tlen - 1) {
				/* Check for e.g. foo matching foo/# */
				if (spos == slen - 3
					&& sub[spos + 1] == '/'
					&& sub[spos + 2] == '#') {
					*result = true;
					multilevel_wildcard = true;
					return MOSQ_ERR_SUCCESS;
				}
			}
			spos++;
			tpos++;
			if (spos == slen && tpos == tlen) {
				*result = true;
				return MOSQ_ERR_SUCCESS;
			}
			else if (tpos == tlen && spos == slen - 1 && sub[spos] == '+') {
				if (spos > 0 && sub[spos - 1] != '/') {
					*result = false;
					return MOSQ_ERR_INVAL;
				}
				spos++;
				*result = true;
				return MOSQ_ERR_SUCCESS;
			}
		}
		else {
			if (sub[spos] == '+') {
				/* Check for bad "+foo" or "a/+foo" subscription */
				if (spos > 0 && sub[spos - 1] != '/') {
					*result = false;
					return MOSQ_ERR_INVAL;
				}
				/* Check for bad "foo+" or "foo+/a" subscription */
				if (spos < slen - 1 && sub[spos + 1] != '/') {
					*result = false;
					return MOSQ_ERR_INVAL;
				}
				spos++;
				while (tpos < tlen && topic[tpos] != '/') {
					tpos++;
				}
				if (tpos == tlen && spos == slen) {
					*result = true;
					return MOSQ_ERR_SUCCESS;
				}
			}
			else if (sub[spos] == '#') {
				if (spos > 0 && sub[spos - 1] != '/') {
					*result = false;
					return MOSQ_ERR_INVAL;
				}
				multilevel_wildcard = true;
				if (spos + 1 != slen) {
					*result = false;
					return MOSQ_ERR_INVAL;
				}
				else {
					*result = true;
					return MOSQ_ERR_SUCCESS;
				}
			}
			else {
				*result = false;
				return MOSQ_ERR_SUCCESS;
			}
		}
	}
	if (multilevel_wildcard == false && (tpos < tlen || spos < slen)) {
		*result = false;
	}

	return MOSQ_ERR_SUCCESS;
}

#ifdef REAL_WITH_TLS_PSK
int _mosquitto_hex2bin(const char *hex, unsigned char *bin, int bin_max_len)
{
	BIGNUM *bn = NULL;
	int len;

	if (BN_hex2bn(&bn, hex) == 0) {
		if (bn) BN_free(bn);
		return 0;
	}
	if (BN_num_bytes(bn) > bin_max_len) {
		BN_free(bn);
		return 0;
	}

	len = BN_bn2bin(bn, bin);
	BN_free(bn);
	return len;
}
#endif

FILE *_mosquitto_fopen(const char *path, const char *mode, bool restrict_read)
{
#ifdef WIN32
	char buf[4096];
	int rc;
	rc = ExpandEnvironmentStrings(path, buf, 4096);
	if (rc == 0 || rc > 4096) {
		return NULL;
	}
	else {
		if (restrict_read) {
			HANDLE hfile;
			SECURITY_ATTRIBUTES sec;
			EXPLICIT_ACCESS ea;
			PACL pacl = NULL;
			char username[UNLEN + 1];
			int ulen = UNLEN;
			SECURITY_DESCRIPTOR sd;

			GetUserName(username, &ulen);
			if (!InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION)) {
				return NULL;
			}
			BuildExplicitAccessWithName(&ea, username, GENERIC_ALL, SET_ACCESS, NO_INHERITANCE);
			if (SetEntriesInAcl(1, &ea, NULL, &pacl) != ERROR_SUCCESS) {
				return NULL;
			}
			if (!SetSecurityDescriptorDacl(&sd, TRUE, pacl, FALSE)) {
				LocalFree(pacl);
				return NULL;
			}

			sec.nLength = sizeof(SECURITY_ATTRIBUTES);
			sec.bInheritHandle = FALSE;
			sec.lpSecurityDescriptor = &sd;

			hfile = CreateFile(buf, GENERIC_READ | GENERIC_WRITE, 0,
				&sec,
				CREATE_NEW,
				FILE_ATTRIBUTE_NORMAL,
				NULL);

			LocalFree(pacl);

			int fd = _open_osfhandle((intptr_t)hfile, 0);
			if (fd < 0) {
				return NULL;
			}

			FILE *fptr = _fdopen(fd, mode);
			if (!fptr) {
				_close(fd);
				return NULL;
			}
			return fptr;

		}
		else {
			return fopen(buf, mode);
		}
	}
#else
	if (restrict_read) {
		FILE *fptr;
		mode_t old_mask;

		old_mask = umask(0077);
		fptr = fopen(path, mode);
		umask(old_mask);

		return fptr;
	}
	else {
		return fopen(path, mode);
	}
#endif
}

