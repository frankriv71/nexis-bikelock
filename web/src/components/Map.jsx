import { useEffect } from 'react'
import { MapContainer, TileLayer, Circle, Marker, useMap } from 'react-leaflet'
import L from 'leaflet'
import 'leaflet/dist/leaflet.css'

// Vite breaks Leaflet's default icon URLs — use DivIcon throughout
function makePositionDot(stateClass) {
  return L.divIcon({
    className: '',
    html: `<div class="nexis-dot ${stateClass}"></div>`,
    iconSize: [12, 12],
    iconAnchor: [6, 6],
  })
}

function makeAnchorIcon() {
  return L.divIcon({
    className: '',
    html: `<div class="nexis-anchor"></div>`,
    iconSize: [14, 14],
    iconAnchor: [7, 7],
  })
}

const STATE_TO_CLASS = {
  DISARMED: 's-disarmed',
  ARMING:   's-arming',
  ARMED:    's-armed',
  WARNING:  's-warning',
  ALERT:    's-alert',
}

const FENCE_COLOR = {
  ALERT:   '#b05252',
  WARNING: '#b87040',
  default: '#c9913a',
}

function MapUpdater({ lat, lng }) {
  const map = useMap()
  useEffect(() => {
    map.panTo([lat, lng], { animate: true, duration: 0.8 })
  }, [lat, lng, map])
  return null
}

export default function Map({ lockStatus }) {
  const { state, lat, lng, anchorLat, anchorLng, geofenceRadius = 50 } = lockStatus

  if (!lat || !lng) return (
    <div className="h-full flex items-center justify-center bg-surface text-muted font-mono text-sm">
      waiting for GPS fix…
    </div>
  )

  const dotClass   = STATE_TO_CLASS[state] ?? 's-disarmed'
  const fenceColor = FENCE_COLOR[state] ?? FENCE_COLOR.default
  const hasAnchor  = anchorLat && anchorLng

  return (
    <MapContainer
      center={[lat, lng]}
      zoom={17}
      style={{ height: '100%', width: '100%' }}
      zoomControl={true}
      attributionControl={true}
    >
      <TileLayer
        url="https://{s}.basemaps.cartocdn.com/dark_all/{z}/{x}/{y}{r}.png"
        attribution='&copy; <a href="https://carto.com/">CARTO</a>'
        maxZoom={20}
      />

      {/* Geofence circle around anchor */}
      {hasAnchor && (
        <Circle
          center={[anchorLat, anchorLng]}
          radius={geofenceRadius}
          pathOptions={{
            color:       fenceColor,
            fillColor:   fenceColor,
            fillOpacity: 0.07,
            weight:      1.5,
            dashArray:   '4 4',
          }}
        />
      )}

      {/* Anchor marker — fixed crosshair, only shown when anchor is set */}
      {hasAnchor && (
        <Marker position={[anchorLat, anchorLng]} icon={makeAnchorIcon()} />
      )}

      {/* Current position marker — pulsing dot */}
      <Marker position={[lat, lng]} icon={makePositionDot(dotClass)} />

      <MapUpdater lat={lat} lng={lng} />
    </MapContainer>
  )
}
