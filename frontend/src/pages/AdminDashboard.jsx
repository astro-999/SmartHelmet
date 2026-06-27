import { useState, useEffect } from 'react';
import { useNavigate } from 'react-router-dom';
import { adminAPI } from '../services/api';
import {
  Users, Wifi, AlertTriangle, ShieldAlert, Search,
  ChevronRight, Battery, MapPin, Clock, Activity
} from 'lucide-react';

function timeAgo(dateStr) {
  if (!dateStr) return 'Never';
  const diff = Date.now() - new Date(dateStr).getTime();
  const mins = Math.floor(diff / 60000);
  if (mins < 1) return 'Just now';
  if (mins < 60) return `${mins}m ago`;
  const hrs = Math.floor(mins / 60);
  if (hrs < 24) return `${hrs}h ago`;
  return `${Math.floor(hrs / 24)}d ago`;
}

export default function AdminDashboard() {
  const [stats, setStats] = useState(null);
  const [riders, setRiders] = useState([]);
  const [search, setSearch] = useState('');
  const [loading, setLoading] = useState(true);
  const navigate = useNavigate();

  useEffect(() => {
    Promise.all([
      adminAPI.stats(),
      adminAPI.riders(),
    ]).then(([statsRes, ridersRes]) => {
      setStats(statsRes.data);
      setRiders(ridersRes.data);
    }).catch(() => {})
      .finally(() => setLoading(false));
  }, []);

  const filteredRiders = riders.filter(r => {
    if (!search) return true;
    const q = search.toLowerCase();
    return (
      r.rider_id?.toLowerCase().includes(q) ||
      r.full_name?.toLowerCase().includes(q) ||
      r.username?.toLowerCase().includes(q) ||
      r.email?.toLowerCase().includes(q) ||
      r.device?.device_id?.toLowerCase().includes(q)
    );
  });

  if (loading) {
    return (
      <div className="fade-in" style={{ padding: 40, textAlign: 'center', color: 'var(--text-secondary)' }}>
        Loading admin dashboard...
      </div>
    );
  }

  return (
    <div className="fade-in">
      <div className="page-header">
        <h2>Admin Dashboard</h2>
        <p>System-wide monitoring &amp; rider management</p>
      </div>

      {/* Stats Grid */}
      <div className="stats-grid">
        <div className="stat-card">
          <div className="stat-icon primary"><Users size={24} /></div>
          <div>
            <div className="stat-value">{stats?.total_riders || 0}</div>
            <div className="stat-label">Total Riders</div>
          </div>
        </div>
        <div className="stat-card">
          <div className="stat-icon success"><Wifi size={24} /></div>
          <div>
            <div className="stat-value">{stats?.online_devices || 0}<span className="stat-sub">/{stats?.total_devices || 0}</span></div>
            <div className="stat-label">Devices Online</div>
          </div>
        </div>
        <div className="stat-card">
          <div className="stat-icon warning"><AlertTriangle size={24} /></div>
          <div>
            <div className="stat-value">{stats?.alerts_today || 0}</div>
            <div className="stat-label">Alerts Today</div>
          </div>
        </div>
        <div className="stat-card">
          <div className="stat-icon danger"><ShieldAlert size={24} /></div>
          <div>
            <div className="stat-value">{stats?.critical_alerts || 0}</div>
            <div className="stat-label">Critical Active</div>
          </div>
        </div>
      </div>

      {/* Riders Section */}
      <div className="admin-section">
        <div className="admin-section-header">
          <h3><Users size={20} /> All Riders</h3>
          <div className="search-box">
            <Search size={16} />
            <input
              type="text"
              placeholder="Search riders, IDs, devices..."
              value={search}
              onChange={e => setSearch(e.target.value)}
              className="search-input"
            />
          </div>
        </div>

        <div className="card" style={{ padding: 0, overflow: 'hidden' }}>
          <div className="table-container">
            <table>
              <thead>
                <tr>
                  <th>Rider ID</th>
                  <th>Name</th>
                  <th>Device</th>
                  <th>Battery</th>
                  <th>Location</th>
                  <th>Alerts</th>
                  <th>Last Seen</th>
                  <th></th>
                </tr>
              </thead>
              <tbody>
                {filteredRiders.length > 0 ? filteredRiders.map(rider => (
                  <tr key={rider.rider_id} className="rider-row" onClick={() => navigate(`/admin/riders/${rider.rider_id}`)}>
                    <td>
                      <span className="rider-id-badge">{rider.rider_id}</span>
                    </td>
                    <td>
                      <div className="rider-name-cell">
                        <div className="rider-avatar">
                          {(rider.full_name || 'U')[0].toUpperCase()}
                        </div>
                        <div>
                          <div style={{ fontWeight: 600 }}>{rider.full_name}</div>
                          <div style={{ fontSize: 12, color: 'var(--text-muted)' }}>{rider.email}</div>
                        </div>
                      </div>
                    </td>
                    <td>
                      {rider.device ? (
                        <div style={{ display: 'flex', alignItems: 'center', gap: 8 }}>
                          <span className={`status-dot ${rider.device.status}`} />
                          <span style={{ fontSize: 13 }}>{rider.device.device_id}</span>
                        </div>
                      ) : (
                        <span className="badge badge-danger" style={{ fontSize: 11 }}>No Device</span>
                      )}
                    </td>
                    <td>
                      {rider.device ? (
                        <div className="battery-cell">
                          <div className="battery-bar">
                            <div
                              className={`battery-fill ${rider.device.battery_level > 50 ? 'good' : rider.device.battery_level > 20 ? 'warn' : 'low'}`}
                              style={{ width: `${rider.device.battery_level}%` }}
                            />
                          </div>
                          <span className="battery-text">{rider.device.battery_level}%</span>
                        </div>
                      ) : '—'}
                    </td>
                    <td>
                      {rider.latest_gps ? (
                        <div style={{ display: 'flex', alignItems: 'center', gap: 4, fontSize: 13, color: 'var(--text-secondary)' }}>
                          <MapPin size={13} />
                          {rider.latest_gps.latitude.toFixed(4)}, {rider.latest_gps.longitude.toFixed(4)}
                        </div>
                      ) : (
                        <span style={{ fontSize: 13, color: 'var(--text-muted)' }}>No GPS</span>
                      )}
                    </td>
                    <td>
                      {rider.active_alerts > 0 ? (
                        <span className="badge badge-danger">{rider.active_alerts} active</span>
                      ) : (
                        <span className="badge badge-success">Clear</span>
                      )}
                    </td>
                    <td>
                      <div style={{ display: 'flex', alignItems: 'center', gap: 4, fontSize: 13, color: 'var(--text-muted)' }}>
                        <Clock size={13} />
                        {timeAgo(rider.device?.last_seen)}
                      </div>
                    </td>
                    <td>
                      <button className="btn btn-sm btn-outline" style={{ padding: '4px 8px' }}>
                        <ChevronRight size={16} />
                      </button>
                    </td>
                  </tr>
                )) : (
                  <tr>
                    <td colSpan={8} style={{ textAlign: 'center', padding: 40, color: 'var(--text-muted)' }}>
                      {search ? 'No riders match your search' : 'No riders registered yet'}
                    </td>
                  </tr>
                )}
              </tbody>
            </table>
          </div>
        </div>
      </div>

      {/* Recent Alerts Feed */}
      {stats?.recent_alerts?.length > 0 && (
        <div className="admin-section" style={{ marginTop: 28 }}>
          <h3 style={{ marginBottom: 16, display: 'flex', alignItems: 'center', gap: 8 }}>
            <Activity size={20} style={{ color: 'var(--danger)' }} /> Live Alert Feed
          </h3>
          <div className="card" style={{ padding: 0 }}>
            {stats.recent_alerts.map((alert, i) => (
              <div key={alert.id || i} className="alert-feed-item">
                <div className={`alert-dot ${alert.severity}`} />
                <div style={{ flex: 1, minWidth: 0 }}>
                  <div style={{ display: 'flex', alignItems: 'center', gap: 8, flexWrap: 'wrap' }}>
                    <span style={{ fontWeight: 600, fontSize: 14 }}>{alert.type_display}</span>
                    <span className="rider-id-badge" style={{ fontSize: 11 }}>{alert.rider_id}</span>
                    <span style={{ fontSize: 12, color: 'var(--text-muted)' }}>{alert.rider_name}</span>
                  </div>
                  {alert.message && (
                    <div style={{ fontSize: 12, color: 'var(--text-secondary)', marginTop: 2 }}>{alert.message}</div>
                  )}
                </div>
                <span className={`badge badge-${alert.severity === 'critical' ? 'danger' : alert.severity === 'high' ? 'warning' : 'info'}`}>
                  {alert.severity}
                </span>
                <span style={{ fontSize: 12, color: 'var(--text-muted)', minWidth: 70, textAlign: 'right' }}>
                  {timeAgo(alert.created_at)}
                </span>
              </div>
            ))}
          </div>
        </div>
      )}
    </div>
  );
}
