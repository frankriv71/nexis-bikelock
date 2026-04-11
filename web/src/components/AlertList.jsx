import { useState } from 'react'
import { timeAgo, absoluteTime } from '../utils/time'

const TYPE_STYLE = {
  TAMPERING:       { label: 'TAMPER', color: 'text-orange border-orange' },
  GEOFENCE_BREACH: { label: 'BREACH', color: 'text-red border-red' },
}

function AlertItem({ alert, onAcknowledge }) {
  const [expanded, setExpanded] = useState(false)
  const type = TYPE_STYLE[alert.type] ?? { label: alert.type, color: 'text-muted border-muted' }

  return (
    <div className={`px-3 py-2.5 border-b border-border transition-opacity ${alert.acknowledged ? 'opacity-40' : ''}`}>
      <div className="flex items-start justify-between gap-2 mb-1.5">
        <span className={`font-display text-xs font-semibold px-1.5 py-0.5 border rounded shrink-0 ${type.color}`}>
          {type.label}
        </span>
        <span
          className="font-mono text-xs text-muted shrink-0 cursor-default"
          title={absoluteTime(alert.timestamp)}
        >
          {timeAgo(alert.timestamp)}
        </span>
      </div>

      <div className="font-mono text-xs text-muted space-y-0.5">
        {alert.distFromAnchor > 0 && (
          <div>{alert.distFromAnchor.toFixed(1)} m from anchor</div>
        )}
        {alert.motionEvents > 0 && (
          <div>motion ×{alert.motionEvents}</div>
        )}
      </div>

      {/* Coords revealed on expand */}
      <button
        onClick={() => setExpanded(e => !e)}
        className="mt-1 font-mono text-[10px] text-muted/40 hover:text-muted transition-colors"
      >
        {expanded ? '▲ hide' : '▼ coords'}
      </button>
      {expanded && (
        <div className="font-mono text-xs text-cyan/60 mt-0.5">
          {alert.lat?.toFixed(6)}, {alert.lng?.toFixed(6)}
        </div>
      )}

      {!alert.acknowledged && (
        <button
          onClick={() => onAcknowledge(alert.id)}
          className="mt-2 text-xs font-display tracking-wider text-muted hover:text-text border border-border hover:border-muted rounded px-2 py-0.5 transition-colors"
        >
          ACK
        </button>
      )}
    </div>
  )
}

export default function AlertList({ alerts, onAcknowledge, hasMore, loadMore, totalCount }) {
  const unacked = alerts.filter(a => !a.acknowledged).length

  return (
    <div className="reveal reveal-4 flex flex-col flex-1 min-h-0">
      <div className="flex items-center justify-between px-4 py-2.5 border-b border-border shrink-0">
        <span className="font-display text-xs font-medium text-muted">
          Alerts {totalCount > 0 && <span className="text-muted/50">({totalCount})</span>}
        </span>
        {unacked > 0 && (
          <span className="font-mono text-xs bg-red/10 text-red border border-red/20 rounded px-1.5 py-0.5">
            {unacked} new
          </span>
        )}
      </div>

      {/* Scrollable list with fade indicator */}
      <div className="flex-1 overflow-y-auto relative scroll-fade">
        {alerts.length === 0 ? (
          <div className="flex items-center justify-center h-full text-muted font-mono text-xs">
            no alerts
          </div>
        ) : (
          <>
            {alerts.map(alert => (
              <AlertItem key={alert.id} alert={alert} onAcknowledge={onAcknowledge} />
            ))}
            {hasMore && (
              <button
                onClick={loadMore}
                className="w-full py-2.5 font-mono text-xs text-muted hover:text-text border-t border-border transition-colors"
              >
                load more
              </button>
            )}
          </>
        )}
      </div>
    </div>
  )
}
