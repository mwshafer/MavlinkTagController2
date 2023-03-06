#include "sendStatusText.h"

#include <mavsdk/mavsdk.h>
#include <mavsdk/plugins/mavlink_passthrough/mavlink_passthrough.h>

void sendStatusText(MavlinkOutgoingMessageQueue& outgoingMessageQueue, const char* text, uint8_t severity)
{
    mavlink_message_t       message;
    mavlink_statustext_t    statustext;

    mavsdk::MavlinkPassthrough& mavlinkPassthrough = outgoingMessageQueue.mavlinkPassthrough();

    memset(&statustext, 0, sizeof(statustext));

    statustext.severity = severity;

    strncpy(statustext.text, text, MAVLINK_MSG_STATUSTEXT_FIELD_TEXT_LEN);

    mavlink_msg_statustext_encode(
        mavlinkPassthrough.get_our_sysid(),
        mavlinkPassthrough.get_our_compid(),
        &message,
        &statustext);
    outgoingMessageQueue.addMessage(message);        	
}