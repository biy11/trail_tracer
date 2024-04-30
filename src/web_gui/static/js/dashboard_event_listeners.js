/*
* @(#) dashboard_event_listeners.js 1.8 2024/04/30.
* Copyright (c) 2023 Aberystwyth University.
* All rights reserved.
*/

/**
 * dashboard_event_listeners.js - Dashboard UI handling and robot control.
 * 
 * This is a JavaScript for handeling interactions, dasboard updates and robot controls.
 * This includes initilising the map, robot and user location markers, routing direction 
 * handeling, and updating telemetray data on the dashboard.
 */

// Map and UI Interactions

/**
 * @author Bilal [biy1]
 * 0.1 initial creation
 * 0.2 set-up for ros2
 * 0.3 comunication with ros2
 * 0.4 bugs fixes
 * 0.5 manual display creatoion
 * 0.6 adaptation for manual display
 * 0.7 file storing for manual display
 * 0.8 Added prompt for loading file
 * 0.9 Fixed bugs for loading files.
 * 1.0 Added button to manually move robot via joystick.
 * 1.1 Added prompt for plotted trail.
 * 1.2 Refactored
 * 1.3 renamed to event_listeners.js
 * 1.4 Added comments
 * 1.5 Added sound effects to buttons and toggle.
 * 1.6 Added event listner for "clear-log" button.
 * 1.7 Refactored code - Implemted const variables for DOM elements.
 * 1.8 Added loggerLog() function to log messages to the logger.
 */

var map; // Reference to Leaflet map instance.
var robotMarker; // Marker for the robot's location.
var userLocationMarker; // Marker for the user's location.
var currentRoutingControl = null; // To hold the current routing control object.
var directionsSet = false; // Flag to track if directions are currently set.
var isRecordingActive = false; // Variable to track whether a trail recording is currently active.
var runingTrail = false; //Tracks if the trail is currently runing.
var trailLoaded = false; //Tracks if the trail has been loaded.
var trailPlotting = false; //Flag to track ewather a trail is being plotted or not.
var coordinatesList = []; // To hold the coordinates clicked on the map
var wayPointMarkers = []; // Array to hold markers for plotted points.
var manualMove = false; // Flag to tell if the robot is being moved without a recording.
var trailPaused = false;

// Constants for DOM elemets
const plotTrailBtn = document.getElementById('plot-trail-button');
const pauseBtn = document.getElementById('pause-button');
const loadTrailBtn = document.getElementById('load-trail-button');
const toggleModeBtn = document.getElementById('mode-toggle');
const locateMeBtn = document.getElementById('locate-me-button');
const manualStartBtn = document.getElementById('manual-start');
const manualMoveBtn = document.getElementById('manual-move');
const clearLogBtn = document.getElementById('clear-log');



// Map initialization and event listeners on DOM content load.
document.addEventListener('DOMContentLoaded', () => {
    // Function to disable the different flags if page is reloaded or closed.
    window.addEventListener('beforeunload', function(e){
        if (isRecordingActive || trailPlotting || manualMove || trailLoaded){
            isRecordingActive  = false;
            trailPlotting = false;
            manualMove = false;
            var wasLoaded = trailLoaded;
            trailLoaded = false;
            
            if(wasLoaded){
                controlRobot('pause-trail');
            }
            var message = 'Current trail recording will end if you leave the page. Are you sure you wish to leave?'
            e.returnValue = message;
            return message;
        }
    });

    // Dynamic functionality for "locate-me-button"
    locateMeBtn.addEventListener('click', function() {
        clickSound(); //Play click sound as buton is clicked
        //Cancel direction if direction is set, if not, locate robot.
        if (directionsSet) {
            cancelDirections(); // Function can be found in map_ui.js
        } else {
            locateRobot(); // Function can be found in map_ui.js
        }
    });

    // Mode-toggle event listeners
    toggleModeBtn.addEventListener('click', function(){
        toggleSound(); // Play toggle sound as buton is clicked 
        toggleMode() // Function can be found in gui_script.js
    });

    // Event listener for manual start button, to start and stop recording trail manually.
    manualStartBtn.addEventListener('click', function() {
        clickSound(); //Play click sound as buton is clicked

        // Check if recording is not active to start a new recording
        if (!isRecordingActive) {
            trailNamePrompt.style.display = 'block';
            noEntryError.style.display = 'none';

            toggleModeBtn.disabled = true;
            manualMoveBtn.disabled = true;
            isRecordingActive = true;
        } else {
            // Code to execute when stopping the recording
            controlRobot('manual-stop'); // Signal the backend that the recording has stopped, function can be found in gui_script.js
            isRecordingActive = false; // Update the recording state
    
            // Update UI components
            this.textContent = 'Record Trail'; // Change the button text back to 'Record Trail'
            manualMoveBtn.style.display = 'block'; // Re-appear the 'manual-move' button
            toggleModeBtn.disabled = false;
            manualMoveBtn.disabled = false;
            loggerLog("[Web GUI] [INFO]: MANUAL TRACE RECORDING ENDED");
        }
    });
    
    // Event listener for manual move button, to move the robot manually.
    manualMoveBtn.addEventListener('click', function(){
        clickSound(); //Play click sound as buton is clicked
        if(!manualMove){
            controlRobot('manual-move'); // Sends message to back-end, function can be found in gui_script.js
            this.textContent = "Stop";
            manualMove = true;

            // Disable other buttons
            toggleModeBtn.disabled = true;
            manualStartBtn.style.display = 'none';
            // Log to logger
            loggerLog("[Web GUI] [INFO]: MANUAL MOVE STARTED");
        } else {
            controlRobot('manual-stop'); // Sends message to back-end, function can be found in gui_script.js
            this.textContent = "Manualy Move Robot";
            // Re-enable other buttons
            toggleModeBtn.disabled = false;
            manualStartBtn.style.display = 'block';
            manualMove = false;
            loggerLog("[Web GUI] [INFO]: MANUAL MOVE ENDED");
        }
    });


    // Event listner for plot name button
    plotTrailBtn.addEventListener('click', function() {
        clickSound(); //Play click sound as buton is clicked
        if (!trailPlotting) {
            this.textContent = 'Save'; // Change the button text to "Save"

            wayPointMarkers.forEach(marker => marker.remove()); // Clearing markers from the map when plotting is stopped
            wayPointMarkers = []; // Reset the markers array
            
            // Disable other buttons except 'locate-me-button'
            toggleModeBtn.disabled = true;
            pauseBtn.disabled = true;
            loadTrailBtn.textContent = 'Cancel' 

            map.on('click', mapClickHandler); // Enable map clicking for coordinate collection, function mapClickHandler can be found in map_ui.js
    
            trailPlotting = true; // Set recording active flag
        } else {
            if(coordinatesList.length == 0){
                alert('No Points plotted! Minimun of 1 point to be plotted before being able to save!')
                return;
            }
            // Re-enable other buttons
            toggleModeBtn.disabled = false;
            pauseBtn.disabled = false;
            loadTrailBtn.disabled = true;
            plotNamePrompt.style.display = 'block';
            toggleModeBtn.disabled = true;
            //noPlotNameErrorBtn.style.display = 'none';

            map.off('click', mapClickHandler); // Disable map clicking for coordinate collection, function mapClickHandler can be found in map_ui.js
    
            console.log('Collected Coordinates:', coordinatesList); // Log the collected coordinates to the console
        }
    }); 

    // Event listner for load trail button, to load a trail file to trace.
    loadTrailBtn.addEventListener('click', function() {
        clickSound(); //Play click sound as buton is clicked

        // Check if there are any changes in number of trail files
        if(latestFiles.length > 0 && !arraysEqual(currentFiles, latestFiles)){
            updateCurrentFiles(latestFiles);
            currentFiles = latestFiles; // Update current files 
            latestFiles = []; //Empty lastest fiel list after update
        }
        // Check if the button on "Cancel" mode
        if (this.textContent === 'Cancel') {
            // If the button is in "Cancel" mode, revert the 'plot-trail-button' text and state
            plotTrailBtn.textContent = 'Plot Trail';
            
            // Revert any other changes made for plotting the trail
            wayPointMarkers.forEach(marker => marker.remove());
            wayPointMarkers = [];
            map.off('click', mapClickHandler); // Function mapClickHandler can be found in map_ui.js
            //trailPlotting = false;
            coordinatesList = [];
            clearWaypointMarkers();
            
            // Re-enable disabled buttons due to plotting
            toggleModeBtn.disabled = false;
            pauseBtn.disabled = false;
            trailPlotting = false;
            //emergencyStopBtn.disabled = false;
            
            // Revert the 'load-trail-button' text back to its original
            this.textContent = 'Load Trail';
        } else if(this.textContent == 'End'){ // Check if the button is in "End" mode
            // If the button is in "End" mode and trail is not running, revert the 'plot-trail-button' text and state
            if(trailPaused){
                loadTrailBtn.textContent = 'Load Trail';
                pauseBtn.textContent = 'Pause';
                toggleModeBtn.disabled = false;
                plotTrailBtn.disabled = false;
                emergencyMsg.style.display = 'none';
                plotTrailBtn.style.display = 'block';
                endTracePrompt.style.display = 'none';
                pauseBtn.style.display = 'none'; 
                trailLoaded = false;
                runingTrail = false;
                trailPaused = false;
                clearWaypointMarkers();
                loggerLog("[Web GUI] [INFO]: TRAIL TRACE QUIT");
            }else{
                endTracePrompt.style.display = 'block'; // Display prompt to either confirm or ending the trail or cancel.
            }
        } else if(!trailLoaded){ // Check if the trail is not loaded
            // If the trail is not loaded, display the prompt to load the trail
            trailPrompt.style.display = 'block';
            toggleModeBtn.disabled = true;
            plotTrailBtn.disabled = true;
            this.textContent = 'Load Trail';
            trailLoaded = true;
        } else{
            // If the trail is loaded, revert the 'plot-trail-button' text and state
            loadTrailBtn.textContent = 'Load Trail';
            trailPrompt.style.display = 'none';
            toggleModeBtn.disabled = false;
            plotTrailBtn.disabled = false;
            plotTrailBtn.style.display = 'block';
            //emergencyStopBtn.style.display = 'none'; 
            pauseBtn.style.display = 'none'; 

            trailLoaded = false;
            runingTrail = false;
            clearWaypointMarkers();
        }
    });

    // Event listner to pause/stop trail tracing
    pauseBtn.addEventListener('click', function() {
        clickSound(); //Play click sound as buton is clicked
        if(!trailPaused){
            //console.log("Paused trail");
            controlRobot('pause-trail');
            this.textContent = "Resume";
            trailPaused = true;
            emergencyMsg.style.display = 'none';
        } else{
            //console.log("trail resumed")          
            controlRobot('resume-trail');
            emergencyMsg.style.display = 'block';
            this.textContent = "Pause";
            trailPaused = false;
        }
    });

    // Event listener for emergency stop featurem which stops the robot and clears the trail.
    document.addEventListener('keydown', function(event){
        if(event.code === "Space" && trailLoaded && !trailPaused){
            event.preventDefault(); // Default is usually a scoll action, this prevents this.
            //console.log("Paused trail");
            controlRobot('pause-trail');
            emergencyMsg.style.display = 'none';
            pauseBtn.textContent = "Resume";
            trailPaused = true;
            loggerLog("[Web GUI] [INFO]: EMERGENCY STOP"); 
        }   
    });
    // Event listener for the clear-log button.
    clearLogBtn.addEventListener('click', ()=> clearLog());
});

