/*
* @(#) gui_script.js 1.5 2024/04/27.
* Copyright (c) 2023 Aberystwyth University.
* All rights reserved.
 */

// WebSocket.js - Handling WebSocket communication
/**
* This JavaScript file manages WebSocket communication between the web GUI and my ROS2 system.
* It enables real-time data exchange for telemetry updates and control commands, supporting both automatic and manual
* navigation modes. The script integrates with a custom communication module for ROS2, facilitating interactions
* like map-based navigation, joystick control, and telemetry display on the dashboard.
*/ 

/**
 * @author Bilal [biy1]
 * @version 0.1 - Initail creation.
 * @version 0.2 - Added sockets for temetry data such as GPS
 * @version 0.3 - Created and added joytsick logic
 * @version 0.4 - Added deadzones to joystick.
 * @version 0.5 - Added trail-file selecter logic.
 * @version 0.6 - Added IMU Data socket listener.
 * @version 0.7 - Added systemlog population button.
 * @version 0.8 - Reduced overhead by only getting files from server when needed.
 * @version 0.9 - Added odom socket to update speed and velocity.
 * @version 1.0 - Added socket for logger.
 * @version 1.1 - Updated logger function.
 * @version 1.2 - Added trail completion message to logger.
 * @version 1.3 - Added tab change capability.
 * @version 1.4 - Added tab change prevention when recording, plotting, or manual move is active. 
 * @version 1.5 - Added comments and cleaned up code.
*/

// WebSocket connection setup.
var mode = 'automatic'
var selectedFileOption = null;
var file
var socket = io.connect('http://' + document.domain + ':' + location.port);
var loggCount = 1;
var currentFiles = []; // Temp stoarge for file names.
var latestFiles = []; // Latest files for comparison reasons.
let previousSelection = null; // Null seclection for trail names
const toggleButton = document.getElementById('mode-toggle');
const automaticControls = document.getElementById('dashboard-controls');
const manualControls = document.getElementById('manual-controls');
const logWindow = document.getElementById("log-window");


// Listen for GPS data from the server.
socket.on('gps_data', function(data) {
    // For debugging purposes
    //console.log("Received GPS data: ", data);
    updateRobotLocation(data.latitude, data.longitude);
    updateDashboard(data.latitude, data.longitude);
});

// Listen for cmd_vel data from the server.
socket.on('odom_data', function(data) {
    // For debugging purposes
    //console.log("Received _odometry data: ", data);
    let odom_linear_velocity = data.linear_velocity;
    let odom_angular_velocity_z = data.angular_velocity_z;
    updateSpeedDashboard(odom_linear_velocity, odom_angular_velocity_z);
});

socket.on('log_info', function(data){
    loggerLog(data.data);
});

socket.on('trail_files_data', function(message) {
    // For debugging purposes
    //console.log("Received trail files data: ", message.data);
    latestFiles = message.data.split(", ");
    //updateCurrentFiles(fileNames);
});

// Function to update the file names in dropdown menu
function updateCurrentFiles(fileNames){
    var trailDropdown = document.getElementById('trail-dropdown');
    // Store current selection for clearing dropdown menu 
    previousSelection = trailDropdown.value; 
    //Update files in dropdown menu
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
    // Check if the arrays are the same length
    if(arr1.length !== arr2.length){
        return false;
    }
    // Check if all elements are the same
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
    // Process the response as JSON
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
    // Process the response as JSON
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
    // Check if the message is a trail completion message
    if(message === "[waypoint_follower] [INFO]: SUCCESS ALL WAYPOINTS REACHED" || message === "[trail_tracer] [INFO]: ENDING CURRENT OPERATION!"){
        loadTrailBtn.textContent = 'Load Trail';
        trailPrompt.style.display = 'none';
        endTracePrompt.style.display = 'none';
        emergencyMsg.style.display = 'none';
        toggleModeBtn.disabled = false;
        plotTrailBtn.disabled = false;
        plotTrailBtn.style.display = 'block';
        pauseBtn.style.display = 'none'; 
        trailLoaded = false;
        runingTrail = false;
        trailPaused = false;
        clearWaypointMarkers();
        controlRobot('pause-trail');
    }
    loggCount++; //Increate log count
}

// Function to clear log and reset counter
function clearLog(){
    logWindow.value = "";
    loggCount = 1;
}

/**
 * Function to change the page content based on the tab clicked.
 * @param {string} pageName The name of the page to display.
 
 */
function openPage(pageName) {
    // Prevent page change if recording, plotting, or manual move is active
    if(isRecordingActive || trailPlotting || manualMove || trailLoaded){
        return;
    }
    // Declare variables for tab content and tab links
    var i, tabcontent, tablinks;
    tabcontent = document.getElementsByClassName("tabcontent");
    // Hide all tab content
    for (i = 0; i < tabcontent.length; i++) {
        tabcontent[i].style.display = "none";
    }
    // Get all tab links and remove the 'active' class
    tablinks = document.getElementsByClassName("tab");
    for (i = 0; i < tablinks.length; i++) {
        tablinks[i].className = tablinks[i].className.replace(" active", "");
    }
    // Display the selected tab content and add an 'active' class to the tab link
    document.getElementById(pageName).style.display = "block";
    document.querySelector("[data-page='" + pageName + "']").className += " active";
    // Hide header for traethlin page
    var header = document.querySelector('header');
    if(pageName === 'traethlin'){
        header.style.display = 'none';
    } else{
        header.style.display = 'block';
    }
}


// Initial tab setup
document.addEventListener('DOMContentLoaded', function() {
    openPage('trailTracer'); // Default page
});