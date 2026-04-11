import { absoluteTime } from '../utils/time'

const STATE_CONFIG = {
  DISARMED: { label: 'DISARMED', color: 'text-muted',  dot: 's-disarmed', ring: '' },
  ARMING:   { label: 'ARMING',   color: 'text-amber',  dot: 's-arming',   ring: 'pulse-amber' },
  ARMED:    { label: 'ARMED',    color: 'text-green',  dot: 's-armed',    ring: 'pulse-green' },
  WARNING:  { label: 'WARNING',  color: 'text-orange', dot: 's-warning',  ring: 'pulse-amber' },
  ALERT:    { label: 'ALERT',    color: 'text-red',    dot: 's-alert',    ring: 'pulse-red' },
}

const ONLINE_THRESHOLD_MS = 60_000

function ConnectionBadge({ lastSeen }) {
  if (!lastSeen) return null
  const stale = Date.now() - new Date(lastSeen) > ONLINE_THRESHOLD_MS
  return stale ? (
    <span className="font-mono text-xs text-red font-medium">OFFLINE</span>
  ) : (
    <span className="font-mono text-xs text-green/70">online</span>
  )
}

export default function StatusPanel({ lockStatus }) {
  const { state, lat, lng, geofenceRadius, lastSeen, wifiSSID, ip } = lockStatus
  const cfg = STATE_CONFIG[state] ?? STATE_CONFIG.DISARMED

  return (
    <div className="reveal reveal-2 shrink-0 px-4 pt-4 pb-3 border-b border-border space-y-3">

      <div className="flex items-center justify-between">
        <div className="flex items-center gap-2.5">
          <div className={`nexis-dot ${cfg.dot} ${cfg.ring}`} />
          <span className={`font-display text-2xl font-bold ${cfg.color}`}>{cfg.label}</span>
        </div>
        <ConnectionBadge lastSeen={lastSeen} />
      </div>

      {lat && lng ? (
        <div className="font-mono text-sm leading-relaxed">
          <div>{lat.toFixed(6)}, {lng.toFixed(6)}</div>
          {geofenceRadius && (
            <div className="text-muted text-xs mt-0.5">{geofenceRadius}m geofence</div>
          )}
        </div>
      ) : (
        <div className="font-mono text-sm text-muted">no GPS fix</div>
      )}

      <div className="font-mono text-xs text-muted flex items-center gap-1.5">
        {wifiSSID && <span>{wifiSSID}</span>}
        {wifiSSID && ip && <span className="opacity-30">·</span>}
        {ip && <span className="text-cyan/70">{ip}</span>}
      </div>

      {lastSeen && (
        <div className="font-mono text-xs text-muted/50" title={absoluteTime(lastSeen)}>
          updated {absoluteTime(lastSeen)}
        </div>
      )}

    </div>
  )
}
