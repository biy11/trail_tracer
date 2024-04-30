/*
* @(#) settings.js 0.5 2024/02/27.
* Copyright (c) 2023 Aberystwyth University.
* All rights reserved.
 */

/**
* settings.js - JavaScript file for handling settings and sound functions
* 
* This is a JavaScript for handling settings and sound functions. This includes 
* handling the settings menu, sound toggling, and playing sound effects.
*
* @author Bilal [biy1]
* @version 1.0 - initial cretaion 
* @version 0.2 - added sound functions
* @version 0.3- Refactored code
* @version 0.4 - Added event liteners for sound-toggle 
* @version 0.5 - cleaned up code and added comments.
*/


var soundOnOff = false; // Variable to store the sound state
// Constants for the settings button, settings menu, and sound toggle...
const settingsButton = document.getElementById('settings-button'); 
const settingsMenu = document.getElementById('settings-menu');
const soundToggle = document.getElementById('sound-toggle');
const audio = document.getElementById('click');
const toggle_audio = document.getElementById('toggle_sound');
const check_audio = document.getElementById('check_sound');

// Function to disable the settings button when certain conditions are met
document.addEventListener('DOMContentLoaded', function() {
    function disableSettingsButton(){
        // Assuming isRecordingActive, trailPlotting, and manualMove are previously defined and their states managed elsewhere
        return isRecordingActive || trailPlotting || manualMove || trailLoaded;
    }
    // Event listener for the settings button
    settingsButton.addEventListener('click', function(event) {
        clickSound(); // Play click sound
        if(disableSettingsButton()){
            event.preventDefault();
            console.log('Settings cannot be used at the moment');
        } else {
            // Toggle the display of the settings menu
            settingsMenu.style.display = settingsMenu.style.display === 'block' ? 'none' : 'block';
        }
    });

    // Hide settings menu when clicking anywhere outside the menu
    window.addEventListener('click', function(event) {
        if (!settingsButton.contains(event.target) && !settingsMenu.contains(event.target)) {
            settingsMenu.style.display = 'none';
        }
    });

    // Sound-toggle event listener
    soundToggle.addEventListener('click', function() {
        soundOnOff = !soundOnOff;

        this.classList.toggle('active');
        if(this.classList.contains('active')){
            clickSound(); //Play click sound
            loggerLog("Sound On")
            soundToggle.textContent = 'Sound On';
        } else {
            loggerLog("Sound off")
            soundToggle.textContent = 'Sound Off';
        }
    });
});
// Function to play click sound
function clickSound(){
    if(soundOnOff){
        if(audio){
            audio.play();
        }
    }
}
// Function to play toggle sound
function toggleSound(){
    if(soundOnOff){
        if(toggle_audio){
            audio.play();
        }
    }   
}
// Function to play check sound
function checkSound(){
    if(soundOnOff){
        if(check_audio){
            audio.play();
        }
    }
}
