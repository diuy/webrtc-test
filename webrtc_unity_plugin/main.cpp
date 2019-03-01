
#include <stdio.h>

#include "examples/unityplugin/unity_plugin_apis.h"

int main(int argc, char* argv[]) {
	const char* urls []= { "stun:stun.l.google.com:19302" };
	int k = CreatePeerConnection(urls, 1, "", "", false);
	ClosePeerConnection(k);
	printf("%d", 1);
}