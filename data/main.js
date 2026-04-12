var received_data;
var main_checkbox, timer_checkbox;
var time_div, system_ip_addr, wifi_ssid, wifi_rssi;
var system_time, system_time_elem;
var to_time, from_time, temp_now, temp_set_value;
var temp_save_button, timer_save_button;

const time_formatter = new Intl.DateTimeFormat('en-GB', {
    timeZone: 'America/Argentina/Buenos_Aires',
    year: 'numeric',
    month: '2-digit',
    day: '2-digit',
    hour: '2-digit',
    minute: '2-digit',
    hour12: false
});

const query_types = {
    "QUERY_STATUS":             0,
    "QUERY_MAIN_OUTPUT_TOGGLE": 1,
    "QUERY_TIMER_TOGGLE":       2,
    "QUERY_TIMER_SET_VALUES":   3,
    "QUERY_TEMP_SET":           4
}

function rssi_to_percentage(rssi) {
    var quality = 0; 

    if (rssi <= -100) { 
        quality = 0; 
    } else if (rssi >= -50) { 
        quality = 100; 
    } else { 
        quality = 2 * (rssi + 100); 
    } 
    return quality; 
}

function update_checkbox() {
    main_checkbox.setAttribute("checked", received_data["main-output-enabled"]);
    timer_checkbox.setAttribute("checked", received_data["timer"]["enabled"]);
}

function update_time() {
    system_time = received_data["system-time"];
    system_time_elem.innerHTML = time_formatter.format(new Date(system_time*1000))
        .replaceAll("/", "-")
        .replaceAll(",", "") + " GMT-3";
}

function update_now_temp() {
    temp_now.textContent = received_data["temp-now"] + "°C";
}

function update_temp() {
    temp_now.textContent = received_data["temp-now"] + "°C";
    temp_set_value.value = received_data["temp-set"];
}

function update_timer() {
    from_time.value = received_data["timer"]["from"]["hour"] + ":"
        + received_data["timer"]["from"]["minute"];
    to_time.value = received_data["timer"]["to"]["hour"] + ":"
        + received_data["timer"]["to"]["minute"];
}

function update_all() {
    update_checkbox();
    update_time();
    update_timer();
    update_temp();

    system_ip_addr.href = received_data["system-ip-addr"];
    system_ip_addr.innerHTML = received_data["system-ip-addr"];

    wifi_ssid.innerHTML = received_data["wifi-ssid"];
    const rssi = received_data["wifi-rssi"];
    wifi_rssi.innerHTML = rssi + "dBm (" + rssi_to_percentage(rssi) + "%)";
}

function socket_onmessage_handler(event) {
    received_data = JSON.parse(event.data);
    // console.log(received_data);

    switch(received_data["type"]) {
        case "all":
            update_all();
            break;
        case "time":
            update_time();
            break;
        case "cb":
            update_checkbox();
            break;
        case "timer":
            update_timer();
            break;
        case "temp-values":
            update_temp();
            break;
        case "temp-now-values":
            update_now_temp();
            break;
        default:
            console.log("[SOCKET] received_data: type not known, updating all...");
            update_all();
    }
}

function socket_onopen_handler(event) {
    console.log("[SOCKET] Connection established")
    socket.send(query_types["QUERY_STATUS"]);
}

function handle_click(cb) {
    if(cb == 'main-toggle') {
        socket.send(query_types["QUERY_MAIN_OUTPUT_TOGGLE"]);
    } else if(cb == 'timer-toggle') {
        socket.send(query_types["QUERY_TIMER_TOGGLE"]);
    } else {
        console.log("handle_click: wrong target");
    }
}

function update_system_time() {
    socket.send(query_types["QUERY_STATUS"]);
}

function timer_set_time() {
    var query_json = new Object();

    query_json.from = new Object();
    query_json.from.hour = from_time.value.slice(0, 2);
    query_json.from.minute = from_time.value.slice(3, 5);

    query_json.to = new Object();
    query_json.to.hour = to_time.value.slice(0, 2);
    query_json.to.minute = to_time.value.slice(3, 5);

    socket.send(query_types["QUERY_TIMER_SET_VALUES"] + JSON.stringify(query_json));
}

function temp_set() {
    var query_json = new Object();

    query_json.temp_set_point_new = temp_set_value.value.slice(0, 2);
    socket.send(query_types["QUERY_TEMP_SET"] + JSON.stringify(query_json));
}


function query_data() {
    main_checkbox = document.getElementById("main-checkbox");
    timer_checkbox = document.getElementById("timer-checkbox");
    system_time_elem = document.getElementById("system-time");
    system_ip_addr = document.getElementById("system-ip-addr");
    wifi_ssid = document.getElementById("wifi-ssid");
    wifi_rssi = document.getElementById("wifi-rssi");

    from_time = document.getElementById("from-time");
    to_time = document.getElementById("to-time");

    temp_now = document.getElementById("main-now-value");
    temp_set_value = document.getElementById("main-set-value");
    temp_set_value.addEventListener("keydown", (e) => {
        if (e.key === "Enter") {
            temp_set_value.blur();
        }
    });

    temp_save_button = document.getElementById("main-save");
    temp_save_button.addEventListener( "click", temp_set, false);

    timer_save_button = document.getElementById("timer-save");
    timer_save_button.addEventListener( "click", timer_set_time, false);

    // setInterval(update_system_time, 30*1000); // update time every 30 secs
}

const socket = new WebSocket("ws:/" + "/" + location.host + ":81");
socket.onmessage = socket_onmessage_handler;
socket.onopen = socket_onopen_handler;
window.onload = query_data;
