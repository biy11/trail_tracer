// WebSocket.js - Handling WebSocket communication
/**
* This JavaScript file manages WebSocket communication between the web GUI and my ROS2 system.
* It enables real-time data exchange for telemetry updates and control commands, supporting both automatic and manual
* navigation modes. The script integrates with a custom communication module for ROS2, facilitating interactions
* like map-based navigation, joystick control, and telemetry display on the dashboard.
*/ 

//Added deadzones to joystick
// Added trail-file selecter logic
//Added IMU Data socket listener
//Added systemlog population button
//Reduced overhead by only getting files from server when needed

// WebSocket connection setup.
var mode = 'automatic'
var selectedFileOption = null;
var file
var socket = io.connect('http://' + document.domain + ':' + location.port);
var linear_cmd_vel = null
var loggCount = 1;
var currentFiles = []; // Temp stoarge for file names.
var latestFiles = []; // Latest files for comparison reasons.
var linear_cmd_vel = 0; // Default cmd_vel value.

// Listen for GPS data from the server.
socket.on('gps_data', function(data) {
    // For debugging purposes
    //console.log("Received GPS data: ", data);
    updateRobotLocation(data.latitude, data.longitude);
    updateDashboard(data.latitude, data.longitude);
});

// Listen for cmd_vel data from the server. USED FOR DE-BUGGING PURPOSES
socket.on('cmd_vel_data', function(data) {
    // For debugging purposes
    //console.log("Received cmd_vel data: ", data);
    linear_cmd_vel = data.linear_x;
    //updateSpeedDashboard(data.linear_x, data.angular_z);
});

// Listen for cmd_vel data from the server.
socket.on('imu_data', function(data) {
    // For debugging purposes
    //console.log("Received cmd_vel data: ", data);
    updateSpeedDashboard(linear_cmd_vel, data.angular_velocity_z);
});


socket.on('trail_files_data', function(message) {
    // For debugging purposes
    //console.log("Received trail files data: ", message.data);
    latestFiles = message.data.split(", ");
    //updateCurrentFiles(fileNames);
});

// Function to update the file names in dropdown menu
function updateCurrentFiles(fileNames){
    currentFiles = fileNames;
    //Update files in dropdown menu
    var trailDropdown = document.getElementById('trail-dropdown');
    trailDropdown.innerHTML = "";
    // Create and append the blank default option
    var defaultOption = document.createElement('option');
    defaultOption.value = "";
    defaultOption.textContent = "Select File";
    trailDropdown.appendChild(defaultOption);

    // Append the actual file options
    fileNames.forEach(function(fileName) {
        var option = document.createElement('option');
        option.value = fileName;
        option.textContent = fileName;
        trailDropdown.appendChild(option);
    });
    // Reapply the previously selected option if it still exists
    if (fileNames.includes(previousSelection)) {
        trailDropdown.value = previousSelection;
    } else {
        // If the previous selection is no longer valid, reset
        trailDropdown.value = "";
        selectedFileOption = null; // Reset selected option
    }
}

// Function to compare two file arrays for updates
function arraysEqual(arr1, arr2){
    if(arr1.length !== arr2.length){
        return false;
    }
    for (var index=0; index < arr1.length; index++){
        if(arr1[index] != arr2[index])
        return false;
    }
    return true;
}


// Function to initialize the joystick and its event handlers.
function createJoystick() {
    var joystick = nipplejs.create({
        zone: document.getElementById('joystick'),
        mode: 'dynamic',
        position: { left: '50%', top: '50%' },
        color: 'green'
    });

    joystick.on('move', function(evt, data) {
        var forwardBackward = data.vector.y; // Negative is forward, positive is backward
        var leftRight = data.vector.x; // Negative is left, positive is right

        var deadZone = 0.1
        var leftRight = Math.abs(data.vector.x) < deadZone ? 0: data.vector.x;

        // Print joystick state to the console for debugging purposes
        //console.log('Joystick Move:', 'Forward/Backward:', forwardBackward, 'Left/Right:', leftRight);

        // Emit joystick control data to the server using the 'joystick_data' event
        socket.emit('joystick_value', {forwardBackward: forwardBackward, leftRight: leftRight});
    });

    joystick.on('end', function() {
        // Print message when joystick is released
        socket.emit('joystick_value', {forwardBackward: 0, leftRight: 0});
        // For debugging purposes
        //console.log('Joystick Released');
    });
}

// Function to toggle between automatic and manual navigation modes.
function toggleMode() {
    var toggleButton = document.getElementById('mode-toggle');
    var automaticControls = document.getElementById('dashboard-controls');
    var manualControls = document.getElementById('manual-controls');

    // Toggle the display of the control panels
    if (mode === 'automatic') {
        mode = 'manual';
        automaticControls.style.display = 'none';
        manualControls.style.display = 'grid';
        toggleButton.innerText = 'Switch to Automatic Navigation';
        toggleButton.classList.add('active');
        createJoystick();
    } else {
        mode = 'automatic';
        automaticControls.style.display = 'grid';
        manualControls.style.display = 'none';
        toggleButton.innerText = 'Switch to Manual Navigation';
        toggleButton.classList.remove('active');
    }
}

function saveTrail(plotName, coordinatesList) {
    // Prepare the data to be sent
    const dataToSend = {
        plotName: plotName,
        coordinates: coordinatesList.map(coord => [coord.lat, coord.lng]) // Convert to array of [lat, lng]
    };

    // Use fetch to send the data to the server endpoint you've defined
    fetch('/save-trail', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',
        },
        body: JSON.stringify(dataToSend),
    })
    .then(response => response.json())
    .then(data => console.log('Success:', data))
    .catch((error) => console.error('Error:', error));
}

// Function to check if a trail file exists on the server.
function checkTrailExists(plotName) {
    // Prepare data to send with the POST request.
    const dataToSend = { plotName: plotName };
    // Send a POST request and check if the file exists.
    return fetch('/check-trail-exists', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json', },
        body: JSON.stringify(dataToSend),
    })
    .then(response => response.json()) // Process JSON reponse
    .then(data => {
            // Log if the file exists or not using a ternary operator
            console.log(data.exists ? "File exitst" : "File does not exist");
            return data.exists;
        }).catch(error => {
            // Log errors that occur during fetch operation.
            console.error('Error:', error);
            return false; // Assume file does not exists due to error.
        });
}

//  This function to emit robot control commands to the server.
function controlRobot(command) {
    // For debugging purposes
    //console.log(`${command} button clicked`);
    socket.emit('web_message', {data: command});
}

// Function to emit trail name to trail_name_topic.
function trail_name_emitter(name){
    // For debugging purposes
    //console.log(`${name} button clicked`);
    socket.emit('trail_name_topic', {data: name})
}

////////////////////////////////////////////////////////////////////////////
//////////////////////////DASHBOARD DATA FUNCTIONS//////////////////////////
////////////////////////////////////////////////////////////////////////////
function updateDashboard(lat, lng) {
    document.getElementById('latitude').textContent = lat.toFixed(6);
    document.getElementById('longitude').textContent = lng.toFixed(6);
    document.getElementById('manual-latitude').textContent = lat.toFixed(6);
    document.getElementById('manual-longitude').textContent = lng.toFixed(6);

}

function updateSpeedDashboard(linear_speed, angular_speed) {
    document.getElementById('linearSpeed').textContent = ` ${linear_speed.toFixed(2)} m/s`;
    document.getElementById('angularSpeed').textContent = `${angular_speed.toFixed(2)} rad/s`;
    document.getElementById('manual-linearSpeed').textContent = ` ${linear_speed.toFixed(2)} m/s`;
    document.getElementById('manual-angularSpeed').textContent = `${angular_speed.toFixed(2)} rad/s`;
}
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////


// Function to logg data in log footer.
function loggerLog(message){
    //assign variable for element ID
    var logWindow = document.getElementById("log-window");

    //Add message to the logg window
    logWindow.value += loggCount + ". "+ message + '\n';
    logWindow.scrollTop = logWindow.scrollHeight; // Scoll down as messages are added 

    loggCount++; //Increate log count
}
// Function to clear log
function clearLog(){
    var logWindow = document.getElementById("log-window");

    //Clear content
    logWindow.value = "";

    // Reset counter
    loggCount = 1;
}