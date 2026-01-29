#!/usr/bin/env python3
"""
Generate improved demo.nmea file with realistic glider circling scenario.

Scenario:
- Glider (type 1, ID 40AB21) starts 5km North, 500m above
- Speed: 100 km/h (27.78 m/s)
- Descends at -3 m/s while approaching
- After ~1km turns 90° right (East)
- Circles with 100m radius while climbing 3-5 m/s (variable)
- After 3 circles, flies away to 10km distance
"""

import math

# NMEA checksum calculator
def nmea_checksum(sentence):
    """Calculate NMEA checksum (XOR of all bytes between $ and *)"""
    checksum = 0
    for char in sentence[1:]:  # Skip the $
        if char == '*':
            break
        checksum ^= ord(char)
    return f"{checksum:02X}"

def create_nmea_sentence(sentence_without_checksum):
    """Add checksum to NMEA sentence"""
    checksum = nmea_checksum(sentence_without_checksum)
    return f"{sentence_without_checksum}*{checksum}"

# Constants
OWNSHIP_LAT = -48.875630  # 4852.5378S in decimal degrees
OWNSHIP_LON = -0.393333  # 12323.6000W in decimal degrees
OWNSHIP_ALT = 500.0  # meters MSL

# Primary Glider
SPEED_KMH = 100.0  # km/h
SPEED_MS = SPEED_KMH / 3.6  # 27.78 m/s
CIRCLE_RADIUS = 100.0  # meters
GLIDER_ID = "40AB21!FLR_40AB21"
AIRCRAFT_TYPE = 1  # Glider

# Paraglider 1 (South-West, sinking)
PG1_ID = "A1B2C3!FAN_A1B2C3"
PG1_SPEED_KMH = 40.0  # km/h
PG1_SPEED_MS = PG1_SPEED_KMH / 3.6  # 11.11 m/s

# Paraglider 2 (North-West, circling)
PG2_ID = "D4E5F6!FAN_D4E5F6"
PG2_SPEED_KMH = 35.0  # km/h
PG2_SPEED_MS = PG2_SPEED_KMH / 3.6  # 9.72 m/s
PG2_CIRCLE_RADIUS = 50.0  # meters

# Phase timing (in seconds at 1Hz updates)
PHASE1_APPROACH_DURATION = 60  # Short approach
PHASE2_TURN_DURATION = 10  # Quick turn
PHASE3_CIRCLE_DURATION = 2 * 20  # 2 circles (shorter demo)
PHASE4_DEPARTURE_MAX_DURATION = 120  # 2 minutes with paragliders (ends after 120 seconds)
PHASE5_FULL_TURN_DURATION = 20  # Full 360° turn in 20 seconds, climbing at 2 m/s

def lat_lon_to_nmea(lat, lon):
    """Convert decimal degrees to NMEA format (DDMM.MMMM)"""
    lat_deg = abs(int(lat))
    lat_min = (abs(lat) - lat_deg) * 60.0
    lat_dir = 'N' if lat >= 0 else 'S'

    lon_deg = abs(int(lon))
    lon_min = (abs(lon) - lon_deg) * 60.0
    lon_dir = 'E' if lon >= 0 else 'W'

    return f"{lat_deg:02d}{lat_min:07.4f}", lat_dir, f"{lon_deg:03d}{lon_min:07.4f}", lon_dir

def meters_to_lat_lon_offset(north_m, east_m, lat):
    """Convert meters North/East to lat/lon offset"""
    # At this latitude, 1 degree lat ≈ 111,320 meters
    # 1 degree lon ≈ 111,320 * cos(latitude) meters
    lat_offset = north_m / 111320.0
    lon_offset = east_m / (111320.0 * math.cos(math.radians(lat)))
    return lat_offset, lon_offset

def generate_demo_file():
    """Generate the complete demo.nmea file"""

    lines = []
    timestamp = 0  # Start time in seconds

    # Phase 1: Approach from 3km North, descending
    print("Phase 1: Approaching from 3km North...")
    north_start = 3000.0  # 3km
    east_pos = 0.0
    altitude = 500.0  # Start 500m above ownship

    for i in range(PHASE1_APPROACH_DURATION):
        # Update position
        north_pos = north_start - (SPEED_MS * i)

        # Descend at -3 m/s
        altitude = 500.0 - (3.0 * i)
        if altitude < 0:
            altitude = 0

        # Generate NMEA sentences
        lines.extend(generate_frame(timestamp, north_pos, east_pos, altitude, 180, -3.0))  # Heading 180 (South), descending -3 m/s
        timestamp += 1

        # Stop at ~1km
        if north_pos <= 1000:
            break

    # Phase 2: Turn 90° right (East)
    print("Phase 2: Turning right (East)...")
    turn_start_north = north_pos
    turn_start_east = east_pos

    for i in range(PHASE2_TURN_DURATION):
        # Calculate turn (90° over 10 seconds)
        angle = 180 + (90 * i / PHASE2_TURN_DURATION)  # 180 to 270 degrees

        # Continue moving during turn
        north_pos = turn_start_north - (SPEED_MS * math.cos(math.radians(angle)) * i)
        east_pos = turn_start_east + (SPEED_MS * math.sin(math.radians(angle)) * i)

        # Continue descending slower
        altitude -= 1.0
        if altitude < 100:
            altitude = 100

        lines.extend(generate_frame(timestamp, north_pos, east_pos, altitude, int(angle) % 360, -1.0))  # Descending -1 m/s during turn
        timestamp += 1

    # Phase 3: Circle while climbing
    print("Phase 3: Circling while climbing...")
    circle_center_north = north_pos
    circle_center_east = east_pos
    circle_start_altitude = altitude

    # Calculate angular velocity: speed / radius (rad/s)
    angular_velocity = SPEED_MS / CIRCLE_RADIUS

    cumulative_altitude = 0.0  # Track total altitude gained

    for i in range(PHASE3_CIRCLE_DURATION):
        # Calculate position on circle (starting from East, going counter-clockwise)
        angle_rad = (270 * math.pi / 180) + (angular_velocity * i)  # Start heading East (270°)

        north_pos = circle_center_north + CIRCLE_RADIUS * math.cos(angle_rad)
        east_pos = circle_center_east + CIRCLE_RADIUS * math.sin(angle_rad)

        # Variable climb rate 3-5 m/s (sinusoidal variation)
        climb_rate = 4.0 + math.sin(angle_rad * 2) * 1.0  # Varies between 3-5 m/s

        # Add this frame's altitude gain
        cumulative_altitude += climb_rate
        altitude = circle_start_altitude + cumulative_altitude

        # Heading is tangent to circle (perpendicular to radius)
        heading = int((angle_rad * 180 / math.pi + 90) % 360)

        # Pass climb rate directly in m/s
        lines.extend(generate_frame(timestamp, north_pos, east_pos, altitude, heading, climb_rate))
        timestamp += 1

    # Phase 4: Fly away to 10km (with two paragliders appearing)
    print("Phase 4: Flying away to 10km (paragliders appear at 1.2km)...")
    departure_start_north = north_pos
    departure_start_east = east_pos
    departure_altitude = altitude

    # Paraglider 1 initial position: 500m South-West, +50m altitude, sinking
    pg1_north_start = -500.0 / math.sqrt(2)  # SW direction
    pg1_east_start = -500.0 / math.sqrt(2)
    pg1_altitude_start = 50.0  # 50m above ownship (will appear red)
    pg1_track = 0  # North (parallel to ownship course 0°)

    # Paraglider 2 initial position: 600m North-East, -750m altitude, circling
    pg2_north_start = 600.0 / math.sqrt(2)  # NE direction
    pg2_east_start = 600.0 / math.sqrt(2)
    pg2_altitude_start = -200.0  # 200m below ownship (will appear green)
    pg2_circle_center_north = pg2_north_start
    pg2_circle_center_east = pg2_east_start
    pg2_cumulative_altitude = 0.0
    pg2_angular_velocity = PG2_SPEED_MS / PG2_CIRCLE_RADIUS

    paragliders_active = False

    for i in range(PHASE4_DEPARTURE_MAX_DURATION):
        # Primary glider: Fly East
        north_pos = departure_start_north
        east_pos = departure_start_east + (SPEED_MS * i)
        altitude = departure_altitude + (2.0 * i)

        # Calculate glider distance to determine when to activate paragliders
        glider_distance = math.sqrt(north_pos**2 + east_pos**2)

        aircraft_list = [{
            'north_m': north_pos,
            'east_m': east_pos,
            'altitude_m': altitude,
            'track_deg': 90,
            'climb_rate': 2.0,
            'aircraft_id': GLIDER_ID,
            'speed_kmh': SPEED_KMH,
            'aircraft_type': AIRCRAFT_TYPE
        }]

        # Activate paragliders when glider reaches 2km distance and is heading away
        if glider_distance >= 1200:
            if not paragliders_active:
                print(f"  Paragliders appearing at timestamp {timestamp}...")
                paragliders_active = True

            pg_time = i - int((2000 - departure_start_north) / SPEED_MS)  # Time since PGs appeared

            # Paraglider 1: Flying North, sinking
            pg1_north = pg1_north_start + (PG1_SPEED_MS * pg_time)
            pg1_east = pg1_east_start
            pg1_altitude = pg1_altitude_start - (1.1 * pg_time)  # Sinking at -1.1 m/s

            aircraft_list.append({
                'north_m': pg1_north,
                'east_m': pg1_east,
                'altitude_m': pg1_altitude,
                'track_deg': pg1_track,
                'climb_rate': -1.1,
                'aircraft_id': PG1_ID,
                'speed_kmh': PG1_SPEED_KMH,
                'aircraft_type': 7  # Paraglider
            })

            # Paraglider 2: Circling and climbing (until 2000m altitude)
            # Calculate position on circle
            angle_rad = pg2_angular_velocity * pg_time

            pg2_north = pg2_circle_center_north + PG2_CIRCLE_RADIUS * math.cos(angle_rad)
            pg2_east = pg2_circle_center_east + PG2_CIRCLE_RADIUS * math.sin(angle_rad)

            # Variable climb rate -0.5 to 3.5 m/s (sinusoidal)
            pg2_climb_rate = 1.5 + math.sin(angle_rad * 2) * 2.0  # Varies between -0.5 and 3.5 m/s

            # Add this frame's altitude gain
            pg2_cumulative_altitude += pg2_climb_rate
            pg2_altitude = pg2_altitude_start + pg2_cumulative_altitude

            # Cap altitude at reasonable max (demo will end after 600s)
            if pg2_altitude >= 2000.0:
                pg2_altitude = 2000.0
                pg2_climb_rate = 0.0

            pg2_heading = int((angle_rad * 180 / math.pi + 90) % 360)

            aircraft_list.append({
                'north_m': pg2_north,
                'east_m': pg2_east,
                'altitude_m': pg2_altitude,
                'track_deg': pg2_heading,
                'climb_rate': pg2_climb_rate,
                'aircraft_id': PG2_ID,
                'speed_kmh': PG2_SPEED_KMH,
                'aircraft_type': 7  # Paraglider
            })

        lines.extend(generate_frame_multi(timestamp, aircraft_list))
        timestamp += 1

    # Phase 5: Full 360° turn while climbing at 2 m/s
    print("Phase 5: Full 360° turn while climbing...")
    turn_center_north = north_pos
    turn_center_east = east_pos
    phase5_start_altitude = altitude

    # Angular velocity for full circle in 20 seconds: 360° = 2π radians
    # 2π radians / 20 seconds = π/10 rad/s
    phase5_angular_velocity = 2 * math.pi / PHASE5_FULL_TURN_DURATION

    for i in range(PHASE5_FULL_TURN_DURATION):
        # Calculate heading for full 360° turn (start from current heading, e.g., 90°)
        angle_rad = (90 * math.pi / 180) + (phase5_angular_velocity * i)
        heading = int((angle_rad * 180 / math.pi) % 360)

        # Keep position fixed at turn center (hovering during turn, or tight circle)
        # For a tight circle at current speed, we'll calculate with a very small radius
        # OR keep it at the center for a hover-like turn
        north_pos = turn_center_north + CIRCLE_RADIUS * 0.2 * math.cos(angle_rad)  # Small radius for visual effect
        east_pos = turn_center_east + CIRCLE_RADIUS * 0.2 * math.sin(angle_rad)

        # Climb at constant 2 m/s
        altitude = phase5_start_altitude + (2.0 * i)

        lines.extend(generate_frame(timestamp, north_pos, east_pos, altitude, heading, 2.0))
        timestamp += 1

    # Add ending sentences (no traffic)
    print("Adding ending...")
    for _ in range(4):
        lines.append(create_nmea_sentence("$PFLAU,0,0,0,1,0,,0,,"))
        lines.append(create_nmea_sentence("$PGRMZ,500,M,2"))

    return lines

def generate_frame_multi(timestamp, aircraft_list):
    """
    Generate one complete frame of NMEA sentences with multiple aircraft.

    aircraft_list: list of dicts with keys:
        - north_m, east_m, altitude_m, track_deg, climb_rate
        - aircraft_id, speed_kmh, aircraft_type
    """
    lines = []

    # Convert timestamp to HHMMSS
    hours = (timestamp // 3600) % 24
    minutes = (timestamp // 60) % 60
    seconds = timestamp % 60
    time_str = f"{hours:02d}{minutes:02d}{seconds:02d}"

    # GPGGA - GPS Fix Data (ownship)
    lat_str, lat_dir, lon_str, lon_dir = lat_lon_to_nmea(OWNSHIP_LAT, OWNSHIP_LON)
    gpgga = f"$GPGGA,{time_str},{lat_str},{lat_dir},{lon_str},{lon_dir},1,05,1.6,{OWNSHIP_ALT:.1f},M,0.0,M,,"
    lines.append(create_nmea_sentence(gpgga))

    # GPRMC - Recommended Minimum Specific GPS/TRANSIT Data (ownship)
    gprmc = f"$GPRMC,{time_str},A,{lat_str},{lat_dir},{lon_str},{lon_dir},16.7,0.0,250504,,,"
    lines.append(create_nmea_sentence(gprmc) + "A")  # Mode indicator A = Autonomous

    # GPGSA - GPS DOP and Active Satellites
    gpgsa = "$GPGSA,A,3,3,5,6,12,18,,,,,,,,,16,"
    lines.append(create_nmea_sentence(gpgsa))

    # Generate PFLAA for each aircraft
    max_alarm_level = 0
    closest_distance = 99999
    closest_altitude = 0

    for aircraft in aircraft_list:
        north_m = aircraft['north_m']
        east_m = aircraft['east_m']
        altitude_m = aircraft['altitude_m']
        track_deg = aircraft['track_deg']
        climb_rate = aircraft['climb_rate']
        aircraft_id = aircraft['aircraft_id']
        speed_kmh = aircraft['speed_kmh']
        aircraft_type = aircraft['aircraft_type']

        distance = int(math.sqrt(north_m**2 + east_m**2))

        # Determine alarm level based on distance
        if distance > 2000:
            alarm_level = 0
        elif distance > 1000:
            alarm_level = 1
        elif distance > 500:
            alarm_level = 2
        else:
            alarm_level = 3

        # Track highest alarm for PFLAU
        if alarm_level > max_alarm_level:
            max_alarm_level = alarm_level
            closest_distance = distance
            closest_altitude = int(altitude_m)

        # PFLAA - Data on other moving objects
        pflaa = (f"$PFLAA,{alarm_level},{int(north_m)},{int(east_m)},{int(altitude_m)},"
                 f"2,{aircraft_id},{track_deg},,{int(speed_kmh)},{climb_rate:.1f},{aircraft_type}")
        lines.append(create_nmea_sentence(pflaa))

    # PFLAU - FLARM Operating Status (summary of all traffic)
    pflau = f"$PFLAU,{len(aircraft_list)},0,2,1,{max_alarm_level},0,2,{closest_altitude},{closest_distance}"
    lines.append(create_nmea_sentence(pflau))

    # PGRMZ - Garmin Altitude
    alt_feet = int(OWNSHIP_ALT * 3.28084)
    pgrmz = f"$PGRMZ,{alt_feet},F,2"
    lines.append(create_nmea_sentence(pgrmz))

    return lines

def generate_frame(timestamp, north_m, east_m, altitude_m, track_deg, climb_rate):
    """Generate one complete frame of NMEA sentences (single aircraft - for backward compatibility)"""
    aircraft_list = [{
        'north_m': north_m,
        'east_m': east_m,
        'altitude_m': altitude_m,
        'track_deg': track_deg,
        'climb_rate': climb_rate,
        'aircraft_id': GLIDER_ID,
        'speed_kmh': SPEED_KMH,
        'aircraft_type': AIRCRAFT_TYPE
    }]
    return generate_frame_multi(timestamp, aircraft_list)

# Generate the file
print("Generating improved demo.nmea file...")
print("=" * 60)

output_lines = generate_demo_file()

# Write to file
output_file = "data/demo.nmea"
with open(output_file, 'w', newline='\n') as f:
    for line in output_lines:
        f.write(line + '\n')

print("=" * 60)
print(f"Generated {len(output_lines)} NMEA sentences")
print(f"Output written to: {output_file}")
print("\nScenario summary:")
print(f"  Primary Aircraft:")
print(f"    - Glider ID: {GLIDER_ID}, Type: {AIRCRAFT_TYPE}")
print(f"    - Speed: {SPEED_KMH} km/h ({SPEED_MS:.2f} m/s)")
print(f"    - Phase 1: Approach from 5km North, descending -3 m/s")
print(f"    - Phase 2: Turn 90° right (East)")
print(f"    - Phase 3: Circle {CIRCLE_RADIUS}m radius, climbing 3-5 m/s")
print(f"    - Phase 4: Fly away to 10km")
print(f"    - Phase 5: Full 360° turn in {PHASE5_FULL_TURN_DURATION}s, climbing at 2.0 m/s")
print(f"\n  Paragliders (appear when glider at 2km):")
print(f"    - PG1 ID: {PG1_ID}, 500m SW, +50m altitude, sinking -1.1 m/s")
print(f"    - PG2 ID: {PG2_ID}, 600m NE, -750m altitude, circling & climbing to 2000m")
print(f"\nTotal duration: ~{len(output_lines) // 8} seconds (avg)")
