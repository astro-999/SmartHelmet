import { useState, useEffect } from 'react';
import { useParams, useNavigate } from 'react-router-dom';
import { adminAPI } from '../services/api';
import {
  ArrowLeft, User, Phone, MapPin, Droplets, CreditCard, Shield,
  Activity, Cpu, Battery, Wifi, WifiOff, Clock, CheckCircle, XCircle
} from 'lucide-react';
import { LineChart, Line, XAxis, YAxis, CartesianGrid, Tooltip, ResponsiveContainer, Legend } from 'recharts';
import { MapContainer, TileLayer, Marker, Popup, Polyline } from 'react-leaflet';
import L from 'leaflet';
import 'leaflet/dist/leaflet.css';

const riderIcon = new L.DivIcon({
  html: '<div style="background:#6366f1;width:24px;height:24px;border-radius:50%;border:3px solid #fff;box-shadow:0 0 10px rgba(99,102,241,0.5);"></div>',
  iconSize: [24, 24],
  className: '',
});

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

export default function AdminRiderDetail() {
  const { riderId } = useParams();
  const navigate = useNavigate();
  const [data, setData] = useState(null);
  const [loading, setLoading] = useState(true);
  const [toggling, setToggling] = useState(false);

  useEffect(() => {
    adminAPI.riderDetail(riderId)
      .then(res => setData(res.data))
      .catch(() => navigate('/admin'))
      .finally(() => setLoading(false));
  }, [riderId, navigate]);

  const handleToggleActive = async () => {
    if (!data) return;
    setToggling(true);
    try {
      await adminAPI.updateRider(riderId, { is_active_rider: !data.rider.is_active_rider });
      setData(prev => ({
        ...prev,
        rider: { ...prev.rider, is_active_rider: !prev.rider.is_active_rider }
      }));
    } catch { }
    setToggling(false);
  };

  if (loading) {
    return (
      <div className="fade-in" style={{ padding: 40, textAlign: 'center', color: 'var(--text-secondary)' }}>
        Loading rider details...
      </div>
    );
  }

  if (!data) return null;

  const { rider, device, sensor_data, gps_trail, latest_gps, alerts, alert_stats } = data;

  const chartData = (sensor_data || []).map((s, i) => ({
    name: i,
    X: parseFloat(s.acc_x?.toFixed(3)),
    Y: parseFloat(s.acc_y?.toFixed(3)),
    Z: parseFloat(s.acc_z?.toFixed(3)),
  }));

  const mapCenter = latest_gps
    ? [latest_gps.latitude, latest_gps.longitude]
    : [27.7172, 85.3240];

  const trailCoords = (gps_trail || [])
    .filter(p => p.latitude && p.longitude)
    .map(p => [p.latitude, p.longitude]);

  return (
    <div className="fade-in">
      {/* Header */}
      <div style={{ display: 'flex', alignItems: 'center', gap: 16, marginBottom: 28 }}>
        <button className="btn btn-outline" onClick={() => navigate('/admin')} style={{ padding: '8px 12px' }}>
          <ArrowLeft size={18} />
        </button>
        <div style={{ flex: 1 }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: 12, flexWrap: 'wrap' }}>
            <h2 style={{ fontSize: 24, fontWeight: 700 }}>{rider.full_name}</h2>
            <span className="rider-id-badge" style={{ fontSize: 14 }}>{rider.rider_id}</span>
            <span className={`badge ${rider.is_active_rider ? 'badge-success' : 'badge-danger'}`}>
              {rider.is_active_rider ? 'Active' : 'Inactive'}
            </span>
          </div>
          <p style={{ color: 'var(--text-secondary)', fontSize: 14, marginTop: 2 }}>
            Rider monitoring &amp; detail view
          </p>
        </div>
        <button
          className={`btn btn-sm ${rider.is_active_rider ? 'btn-outline' : 'btn-success'}`}
          onClick={handleToggleActive}
          disabled={toggling}
        >
          {rider.is_active_rider ? (
            <><XCircle size={14} /> Deactivate</>
          ) : (
            <><CheckCircle size={14} /> Activate</>
          )}
        </button>
      </div>

      {/* Rider Info + Device Status */}
      <div className="grid-2" style={{ marginBottom: 24 }}>
        {/* Rider Info Card */}
        <div className="card">
          <h3 className="card-title"><User size={18} /> Rider Information</h3>
          <div className="info-grid">
            <div className="info-item">
              <span className="info-label"><User size={14} /> Username</span>
              <span className="info-value">{rider.username}</span>
            </div>
            <div className="info-item">
              <span className="info-label"><CreditCard size={14} /> Email</span>
              <span className="info-value">{rider.email || '—'}</span>
            </div>
            <div className="info-item">
              <span className="info-label"><Phone size={14} /> Phone</span>
              <span className="info-value">{rider.phone || '—'}</span>
            </div>
            <div className="info-item">
              <span className="info-label"><Droplets size={14} /> Blood Group</span>
              <span className="info-value">{rider.blood_group || '—'}</span>
            </div>
            <div className="info-item">
              <span className="info-label"><CreditCard size={14} /> License</span>
              <span className="info-value">{rider.license_number || '—'}</span>
            </div>
            <div className="info-item">
              <span className="info-label"><MapPin size={14} /> Address</span>
              <span className="info-value">{rider.address || '—'}</span>
            </div>
          </div>
          <div style={{ marginTop: 16, paddingTop: 16, borderTop: '1px solid var(--border)', fontSize: 12, color: 'var(--text-muted)' }}>
            Joined: {new Date(rider.date_joined).toLocaleDateString()}
          </div>
        </div>

        {/* Device Card */}
        <div className="card">
          <h3 className="card-title"><Cpu size={18} /> Device Status</h3>
          {device ? (
            <>
              <div className="info-grid">
                <div className="info-item">
                  <span className="info-label">Device ID</span>
                  <span className="info-value" style={{ fontFamily: 'monospace' }}>{device.device_id}</span>
                </div>
                <div className="info-item">
                  <span className="info-label">Status</span>
                  <span className="info-value">
                    <span style={{ display: 'inline-flex', alignItems: 'center', gap: 6 }}>
                      <span className={`status-dot ${device.status}`} />
                      {device.status === 'online' ? 'Online' : 'Offline'}
                    </span>
                  </span>
                </div>
                <div className="info-item">
                  <span className="info-label"><Battery size={14} /> Battery</span>
                  <span className="info-value">
                    <div className="battery-cell">
                      <div className="battery-bar" style={{ width: 80 }}>
                        <div
                          className={`battery-fill ${device.battery_level > 50 ? 'good' : device.battery_level > 20 ? 'warn' : 'low'}`}
                          style={{ width: `${device.battery_level}%` }}
                        />
                      </div>
                      <span>{device.battery_level}%</span>
                    </div>
                  </span>
                </div>
                <div className="info-item">
                  <span className="info-label">Firmware</span>
                  <span className="info-value">{device.firmware_version}</span>
                </div>
                <div className="info-item">
                  <span className="info-label">BLE MAC</span>
                  <span className="info-value" style={{ fontFamily: 'monospace', fontSize: 12 }}>{device.ble_mac || '—'}</span>
                </div>
                <div className="info-item">
                  <span className="info-label"><Clock size={14} /> Last Seen</span>
                  <span className="info-value">{timeAgo(device.last_seen)}</span>
                </div>
              </div>
              <div style={{ marginTop: 16, paddingTop: 16, borderTop: '1px solid var(--border)', fontSize: 12, color: 'var(--text-muted)' }}>
                Fall Threshold: {device.fall_threshold}g &bull; No-move Timeout: {device.no_move_timeout}ms
              </div>
            </>
          ) : (
            <div style={{ textAlign: 'center', padding: 40, color: 'var(--text-muted)' }}>
              <WifiOff size={32} style={{ marginBottom: 8, opacity: 0.5 }} /><br />
              No device assigned
            </div>
          )}
        </div>
      </div>

      {/* Alert Stats */}
      <div className="stats-grid" style={{ gridTemplateColumns: 'repeat(3, 1fr)', marginBottom: 24 }}>
        <div className="stat-card">
          <div className="stat-icon info"><Shield size={20} /></div>
          <div>
            <div className="stat-value">{alert_stats.total}</div>
            <div className="stat-label">Total Alerts</div>
          </div>
        </div>
        <div className="stat-card">
          <div className="stat-icon danger"><Activity size={20} /></div>
          <div>
            <div className="stat-value">{alert_stats.active}</div>
            <div className="stat-label">Active Alerts</div>
          </div>
        </div>
        <div className="stat-card">
          <div className="stat-icon success"><CheckCircle size={20} /></div>
          <div>
            <div className="stat-value">{alert_stats.resolved}</div>
            <div className="stat-label">Resolved</div>
          </div>
        </div>
      </div>

      {/* Sensor Chart + Map */}
      <div className="grid-2">
        <div className="card">
          <h3 className="card-title">
            <Activity size={18} style={{ color: 'var(--primary)' }} /> Accelerometer Data
          </h3>
          {chartData.length > 0 ? (
            <ResponsiveContainer width="100%" height={250}>
              <LineChart data={chartData}>
                <CartesianGrid strokeDasharray="3 3" stroke="var(--border)" />
                <XAxis dataKey="name" tick={false} stroke="var(--text-muted)" />
                <YAxis stroke="var(--text-muted)" domain={[-3, 3]} />
                <Tooltip
                  contentStyle={{
                    background: 'var(--bg-card)', border: '1px solid var(--border)',
                    borderRadius: 8, color: 'var(--text-primary)'
                  }}
                />
                <Legend />
                <Line type="monotone" dataKey="X" stroke="#6366f1" dot={false} strokeWidth={2} />
                <Line type="monotone" dataKey="Y" stroke="#10b981" dot={false} strokeWidth={2} />
                <Line type="monotone" dataKey="Z" stroke="#f59e0b" dot={false} strokeWidth={2} />
              </LineChart>
            </ResponsiveContainer>
          ) : (
            <div style={{ textAlign: 'center', padding: 40, color: 'var(--text-muted)' }}>
              No sensor data available
            </div>
          )}
        </div>

        <div className="card">
          <h3 className="card-title">
            <MapPin size={18} style={{ color: 'var(--success)' }} /> GPS Location &amp; Trail
          </h3>
          <div className="map-container" style={{ height: 250 }}>
            <MapContainer center={mapCenter} zoom={14} style={{ height: '100%', width: '100%' }} scrollWheelZoom={false}>
              <TileLayer
                attribution='&copy; <a href="https://www.openstreetmap.org/">OSM</a>'
                url="https://{s}.basemaps.cartocdn.com/dark_all/{z}/{x}/{y}{r}.png"
              />
              {latest_gps && (
                <Marker position={[latest_gps.latitude, latest_gps.longitude]} icon={riderIcon}>
                  <Popup>
                    <strong>{rider.full_name}</strong><br />
                    {rider.rider_id}<br />
                    Speed: {latest_gps.speed?.toFixed(1)} km/h
                  </Popup>
                </Marker>
              )}
              {trailCoords.length > 1 && (
                <Polyline
                  positions={trailCoords}
                  pathOptions={{ color: '#6366f1', weight: 3, opacity: 0.7, dashArray: '8 4' }}
                />
              )}
            </MapContainer>
          </div>
        </div>
      </div>

      {/* Alert History */}
      {alerts && alerts.length > 0 && (
        <div className="admin-section" style={{ marginTop: 24 }}>
          <h3 style={{ marginBottom: 16, display: 'flex', alignItems: 'center', gap: 8 }}>
            <Shield size={20} style={{ color: 'var(--danger)' }} /> Alert History
          </h3>
          <div className="card" style={{ padding: 0 }}>
            <div className="table-container">
              <table>
                <thead>
                  <tr>
                    <th>Type</th>
                    <th>Severity</th>
                    <th>Status</th>
                    <th>Message</th>
                    <th>Location</th>
                    <th>Time</th>
                  </tr>
                </thead>
                <tbody>
                  {alerts.map(alert => (
                    <tr key={alert.id}>
                      <td>
                        <div style={{ display: 'flex', alignItems: 'center', gap: 8 }}>
                          <span className={`alert-dot ${alert.severity}`} />
                          {alert.type_display || alert.alert_type}
                        </div>
                      </td>
                      <td>
                        <span className={`badge badge-${alert.severity === 'critical' ? 'danger' : alert.severity === 'high' ? 'warning' : alert.severity === 'medium' ? 'info' : 'success'}`}>
                          {alert.severity}
                        </span>
                      </td>
                      <td>
                        <span className={`badge ${alert.status === 'active' ? 'badge-danger' : alert.status === 'resolved' ? 'badge-success' : 'badge-info'}`}>
                          {alert.status}
                        </span>
                      </td>
                      <td style={{ maxWidth: 200, overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>
                        {alert.message || '—'}
                      </td>
                      <td style={{ fontSize: 12, color: 'var(--text-muted)' }}>
                        {alert.latitude ? `${alert.latitude.toFixed(4)}, ${alert.longitude.toFixed(4)}` : '—'}
                      </td>
                      <td style={{ fontSize: 12, color: 'var(--text-muted)', whiteSpace: 'nowrap' }}>
                        {new Date(alert.created_at).toLocaleString()}
                      </td>
                    </tr>
                  ))}
                </tbody>
              </table>
            </div>
          </div>
        </div>
      )}
    </div>
  );
}
