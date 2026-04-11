import Map         from './components/Map'
import StatusPanel from './components/StatusPanel'
import Controls    from './components/Controls'
import AlertList   from './components/AlertList'
import { useLockStatus }    from './hooks/useLockStatus'
import { useAlerts }        from './hooks/useAlerts'
import { useNotifications } from './hooks/useNotifications'
import { DEMO_MODE }        from './firebase'

const LOCK_ID = 'nexis_001'

const ALERT_BORDER = {
  ALERT:   'border-red/30',
  WARNING: 'border-orange/30',
}

function BellIcon({ enabled }) {
  return enabled ? (
    <svg width="14" height="14" viewBox="0 0 24 24" fill="currentColor">
      <path d="M12 22a2 2 0 0 0 2-2H10a2 2 0 0 0 2 2zm6-6V11a6 6 0 0 0-5-5.91V4a1 1 0 0 0-2 0v1.09A6 6 0 0 0 6 11v5l-2 2v1h16v-1l-2-2z"/>
    </svg>
  ) : (
    <svg width="14" height="14" viewBox="0 0 24 24" fill="currentColor" opacity="0.4">
      <path d="M12 22a2 2 0 0 0 2-2H10a2 2 0 0 0 2 2zm6-6V11a6 6 0 0 0-5-5.91V4a1 1 0 0 0-2 0v1.09A6 6 0 0 0 6 11v5l-2 2v1h16v-1l-2-2z"/>
    </svg>
  )
}

export default function App() {
  const { lockStatus, setRemoteState }          = useLockStatus(LOCK_ID)
  const { alerts, acknowledgeAlert, hasMore, loadMore, totalCount } = useAlerts(LOCK_ID)
  const { enabled: notifEnabled, permission, toggle: toggleNotif } = useNotifications(alerts)

  // lockStatus is null until first Firestore snapshot arrives (production only)
  if (!lockStatus) return (
    <div className="flex h-screen items-center justify-center bg-bg font-mono text-xs text-muted">
      connecting…
    </div>
  )

  const panelBorder = ALERT_BORDER[lockStatus.state] ?? 'border-border'

  return (
    <div className="flex flex-col h-screen overflow-hidden bg-bg text-text">

      <header className="reveal reveal-1 flex items-center justify-between px-5 py-3 border-b border-border shrink-0">
        <div className="flex items-center gap-4">
          <span className="font-display text-xl font-bold tracking-[0.25em] text-amber">NEXIS</span>
          <span className="font-mono text-xs text-muted/60">{LOCK_ID}</span>
          {DEMO_MODE && (
            <span className="font-mono text-[10px] px-1.5 py-0.5 border border-amber/30 text-amber/60 rounded">
              DEMO
            </span>
          )}
        </div>

        <div className="flex items-center gap-3">
          {lockStatus.wifiSSID && (
            <span className="font-mono text-xs text-muted/50">{lockStatus.wifiSSID}</span>
          )}
          <button
            onClick={toggleNotif}
            disabled={permission === 'denied'}
            title={
              permission === 'denied' ? 'Notifications blocked by browser'
              : notifEnabled ? 'Disable notifications'
              : 'Enable notifications'
            }
            className={`flex items-center gap-1.5 px-2 py-1 rounded border transition-colors
              ${notifEnabled
                ? 'border-amber/40 text-amber'
                : 'border-border text-muted hover:border-muted hover:text-text'}
              disabled:cursor-not-allowed disabled:opacity-30`}
          >
            <BellIcon enabled={notifEnabled} />
            <span className="font-mono text-[10px]">{notifEnabled ? 'ON' : 'OFF'}</span>
          </button>
        </div>
      </header>

      <div className="flex flex-1 overflow-hidden">
        <div className="reveal reveal-1 flex-1 relative">
          <Map lockStatus={lockStatus} />
        </div>

        <div className={`flex flex-col w-80 border-l transition-colors duration-500 ${panelBorder} overflow-hidden`}>
          <StatusPanel lockStatus={lockStatus} />
          <Controls
            lockStatus={lockStatus}
            onArm={() => setRemoteState('ARMING')}
            onDisarm={() => setRemoteState('DISARMED')}
          />
          <AlertList
            alerts={alerts}
            onAcknowledge={acknowledgeAlert}
            hasMore={hasMore}
            loadMore={loadMore}
            totalCount={totalCount}
          />
        </div>
      </div>

    </div>
  )
}
