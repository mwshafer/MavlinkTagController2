#include "sendStatusText.h"

void sendStatusText(MavlinkPassthrough& mavlinkPassthrough, const char* text)
{
    mavlink_message_t       message;
    mavlink_statustext_t    statustext;

    memset(&statustext, 0, sizeof(statustext));

    statustext.severity = MAV_SEVERITY_INFO;

    strncpy(statustext.text, text, MAVLINK_MSG_STATUSTEXT_FIELD_TEXT_LEN);

    mavlink_msg_statustext_encode(
        mavlinkPassthrough.get_our_sysid(),
        mavlinkPassthrough.get_our_compid(),
        &message,
        &statustext);
    mavlinkPassthrough.send_message(message);        	
}