/*
* @(#) map_ui.js 1.6 2024/04/30.
* Copyright (c) 2023 Aberystwyth University.
* All rights reserved.
 */

/**
 * map_ui.js -- Map User Interface
 * This is a JavaScript for handeling map interactions. This includes 
 * initilising the map, robot and location markers, routing direction 
 * handeling.
 */

// Map and UI Interactions

/**
 * 0.1 initial creation
 * 0.2 Added map function from map_ui.js to here
 * 0.3 Renamed to map_ui.js
 * 0.4 Added Comments
 * 0.5 Removed auto-marker centering when a plotting is active.
 * 0.6 Fixed bug in calculateAndDisplayRoute function.
 */

// Function to check internet connectivity more reliably
function checkInternet(callback) {
    fetch('https://openstreetmap.org', {
        method: 'HEAD',
        mode: 'no-cors' // To avoiding CORS errors
    }).then(() => {
        callback(true); // Internet available
    }).catch(() => {
        callback(false); // Internet not available or blocked by CORS
    });
}
// initialize the map
initializeMap();

// Function to initialize the map
function initializeMap() {
    map = L.map('map').setView([52.416333, -4.0651128], 19); // Initial map setup with a default location and zoom level
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
    // Add robot marker to map
    robotMarker = L.marker([52.416333, -4.0651128]).addTo(map).bindPopup('Robot Location');
    
    // When map is clicked and there is no active plot or directions set already
    map.on('click', function(e) {
        if(!trailPlotting){
            if (!directionsSet) {
                calculateAndDisplayRoute(e.latlng); // Calculate route on map click if no directions are set.
            }
        }
    });;
}

// Function to locate the robot and set the map-view to its corrent location.
function locateRobot(){
    var robotLatLng = robotMarker.getLatLng();
    var zoom = 18;
    map.setView(robotLatLng, zoom) // Set map-view to robot's location with a zoom of 18.
}

// Function to update the robot's location on the map based on telemetary data.
function updateRobotLocation(lat, lng) {
    var newLatLng = new L.LatLng(lat, lng);
    robotMarker.setLatLng(newLatLng);
    // Unly update map view (Center) if there is no plotting
    if(!trailPlotting){
        map.setView(newLatLng, map.getZoom()); 
    }
}

// Handler function for map clicks
function mapClickHandler(e) {
    var clickedCoordinate = e.latlng;
    coordinatesList.push(clickedCoordinate);
    //console.log('Coordinate added:', clickedCoordinate); For debugging prposes

    // Create a marker for the clicked point
    var wayPointMarker = L.marker(clickedCoordinate, {
        icon: L.divIcon({
            className: 'waypoint-marker',
            html: '<div style="color: red; font-size: 24px; font-weight: bold;">×</div>',
            iconSize: [20, 20]
        })
    }).addTo(map);

    // Add the marker to the array for management
    wayPointMarkers.push(wayPointMarker);
}

// Function to calculate and display routing information on the map.
function calculateAndDisplayRoute(clickedLatLng) {
    var robotLatLng = robotMarker.getLatLng();
    if (currentRoutingControl != null) {
        map.removeControl(currentRoutingControl);
    }
    // Create a new routing control with the robot's location as the starting point and the clicked location as the destination.
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
        var summary = routes[0].summary; // Getting the first found route
        //console.log("Total distance: " + summary.totalDistance + " meters");
        //console.log("Estimated travel time: " + summary.totalTime + " seconds");

        // Log all the coordinates for the route
        var routeCoordinates = routes[0].coordinates;
        //console.log("Route coordinates:");
        routeCoordinates.forEach(function(coord, index) {
            console.log("Point " + (index + 1) + ": Lat =", coord.lat, "Lng =", coord.lng);
        });
    });

    directionsSet = true;
    locateMeBtn.textContent = 'Cancel Directions';
}

// Function to cancel any set directions and resets the map state.
function cancelDirections() {
    if (currentRoutingControl != null) {
        map.removeControl(currentRoutingControl);
        currentRoutingControl = null;
    }
    directionsSet = false;
    locateMeBtn.textContent = 'Locate Me';
}

// Function to remove waypoint markers from map.
function clearWaypointMarkers(){
    waypointMarkers.forEach(function(marker){
        map.removeLayer(marker);
    });
    waypointMarkers = [];
}