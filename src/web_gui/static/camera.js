/**
 * This is a JavaScript for handeling interactions, dasboard updates and robot controls.
 * This includes initilising the map, robot and user location markers, routing direction 
 * handeling, and updating telemetray data on the dashboard.
 */

// Map and UI Interactions

/**
 * 0.1 initial creation
 * 0.2 set-up for ros2
 * 0.3 comunication with ros2
 * 0.4 bugs fixes
 * 0.5 manual display creatoion
 * 0.6 adaptation for manual display
 * 0.7 file storing for manual display
 * 0.8 Added prompt for loading file
 * 0.9 Fixed bugs for loading files.
 */

var map; // Reference to Leaflet map instance.
var robotMarker; // Marker for the robot's location.
var userLocationMarker; // Marker for the user's location.
var currentRoutingControl = null; // To hold the current routing control object.
var directionsSet = false; // Flag to track if directions are currently set.


// Map initialization and event listeners on DOM content load.
document.addEventListener('DOMContentLoaded', () => {
    initializeMap();

    window.addEventListener('beforeunload', function(e){
        if (isRecordingActive){
            var message = 'Current trail recording will end if you leave thepage. Are you sure you wish to leave?'
            e.returnValue = message;
            //controlRobot('manual-stop');
            return message;
        }
    });

    // Dynamic functionality for "locate-me-button"
    document.getElementById('locate-me-button').addEventListener('click', function() {
        if (directionsSet) {
            cancelDirections();
        } else {
            locateRobot();
        }
    });

    document.getElementById('camera-view-button').addEventListener('click', function() {
        var mapContainer = document.getElementById('maps');
        var cameraViewContainer = document.getElementById('camera-view');
        
        // Toggle a class that controls the display
        cameraViewContainer.classList.toggle('active');
    
        // Check if the camera view has the 'active' class
        if (cameraViewContainer.classList.contains('active')) {
            // If the camera view is active, set the flex properties to show it
            mapContainer.style.flex = '1'; // Map takes up only half the width
            cameraViewContainer.style.flex = '1'; // Camera view takes up half the width
            cameraViewContainer.style.display = 'block'; // Ensure it's shown
        } else {
            // If the camera view is not active, hide it and allow the map to take full width
            mapContainer.style.flex = '0 0 100%'; // Map takes full width
            // Optional: If you want to explicitly hide the camera view
            cameraViewContainer.style.display = 'none'; // Hide camera view
        }
    
        // It's important to invalidate the size after the browser has had time to reflow
        setTimeout(function() {
            map.invalidateSize();
        }, 200); // Adjust time to match CSS transition duration
    });
});

// Function to check internet connectivity more reliably
function checkInternet(callback) {
    fetch('https://openstreetmap.org', {
        method: 'HEAD',
        mode: 'no-cors' // To attempt avoiding CORS errors
    })
    .then(() => {
        callback(true); // Internet seems to be available
    })
    .catch(() => {
        callback(false); // Internet is not available or blocked by CORS
    });
}

function initializeMap() {
    map = L.map('maps').setView([52.416333, -4.0651128], 19); // Initial map setup with a default


    // Check internet connectivity before setting tile layer
    checkInternet(function(isConnected) {
        if (isConnected) {
            // If there's internet connection, use OpenStreetMap's tiles
            L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {
                maxZoom: 19,
                attribution: '© OpenStreetMap contributors'
            }).addTo(map);
        } else {
            // If there's no internet connection, use local tiles
            L.tileLayer('/static/tiles/{z}/{x}/{y}.png', {
                maxZoom: 19,
                minZoom: 15,
                attribution: 'Map data © OpenStreetMap contributors'
            }).addTo(map);
        }
    });

    robotMarker = L.marker([52.416333, -4.0651128]).addTo(map).bindPopup('Robot Location');

    map.on('click', function(e) {
        if(!trailPlotting){
            if (!directionsSet) {
                calculateAndDisplayRoute(e.latlng); // Calculate route on map click if no directions are set.
            }
        }
    });;
}

// Function to calculate and display routing information on the map.
function calculateAndDisplayRoute(clickedLatLng) {
    var robotLatLng = robotMarker.getLatLng();
    
    if (currentRoutingControl != null) {
        map.removeControl(currentRoutingControl);
    }

    currentRoutingControl = L.Routing.control({
        waypoints: [
            L.latLng(robotLatLng.lat, robotLatLng.lng),
            L.latLng(clickedLatLng.lat, clickedLatLng.lng)
        ],
        routeWhileDragging: false
    }).addTo(map);

    // Listen for the 'routesfound' event on the routing control
    currentRoutingControl.on('routesfound', function(e) {
        var routes = e.routes;
        var summary = routes[0].summary; // Assuming you want the first found route
        console.log("Total distance: " + summary.totalDistance + " meters");
        console.log("Estimated travel time: " + summary.totalTime + " seconds");

        // Log all the coordinates for the route
        var routeCoordinates = routes[0].coordinates;
        console.log("Route coordinates:");
        routeCoordinates.forEach(function(coord, index) {
            console.log("Point " + (index + 1) + ": Lat =", coord.lat, "Lng =", coord.lng);
        });
    });

    directionsSet = true;
    document.getElementById('locate-me-button').textContent = 'Cancel Directions';
}

// Function to locate the robot and set the map-view to its corrent location.
function locateRobot(){
    var robotLatLng = robotMarker.getLatLng();
    var zoom = 18;
    map.setView(robotLatLng, zoom) // Set map-view to robot's location with a zoom of 18.
}

// Function to cancel any set directions and resets the map state.
function cancelDirections() {
    if (currentRoutingControl != null) {
        map.removeControl(currentRoutingControl);
        currentRoutingControl = null;
    }
    directionsSet = false;
    document.getElementById('locate-me-button').textContent = 'Locate Me';
}


// Function to update the robot's location on the map based on telemetary data.
function updateRobotLocation(lat, lng) {
    var newLatLng = new L.LatLng(lat, lng);
    robotMarker.setLatLng(newLatLng);
    map.setView(newLatLng, map.getZoom());
}
