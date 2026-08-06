#include "view_cube.hpp"

#include <QLineF>
#include <QMouseEvent>
#include <QPainter>
#include <QTimer>

#include <algorithm>
#include <cmath>
#include <vector>

namespace brep::viewer {
namespace {

constexpr float kDeg = 0.01745329252f;

struct ProjFace {
  ViewCubeWidget::FaceId id;
  char code;
  const char* label;
  QPolygonF poly;
  float depth;
  QColor color;
  bool visible;
};

}  // namespace

ViewCubeWidget::ViewCubeWidget(QWidget* parent) : QWidget(parent) {
  setFixedSize(132, 132);
  setAttribute(Qt::WA_TransparentForMouseEvents, false);
  setMouseTracking(true);
  setCursor(Qt::ArrowCursor);
  setToolTip(QStringLiteral("Click a face to snap the view"));

  // Keep the cube in sync while the user orbits the 3D camera.
  auto* timer = new QTimer(this);
  connect(timer, &QTimer::timeout, this, [this] {
    if (!camera_) return;
    if (camera_->yaw_deg != last_yaw_ || camera_->pitch_deg != last_pitch_) {
      last_yaw_ = camera_->yaw_deg;
      last_pitch_ = camera_->pitch_deg;
      update();
    }
  });
  timer->start(33);
}

void ViewCubeWidget::project_point(float x, float y, float z, float& sx,
                                   float& sy, float& depth) const {
  const float yaw = (camera_ ? camera_->yaw_deg : -35.0f) * kDeg;
  const float pitch = (camera_ ? camera_->pitch_deg : -25.0f) * kDeg;
  const float cy = std::cos(yaw);
  const float syw = std::sin(yaw);
  const float cp = std::cos(pitch);
  const float sp = std::sin(pitch);

  // Same basis as Camera::eye / view: rotate world into camera-facing space.
  const float x1 = x * cy + z * syw;
  const float z1 = -x * syw + z * cy;
  const float y2 = y * cp - z1 * sp;
  const float z2 = y * sp + z1 * cp;

  const float scale = 38.0f;
  sx = width() * 0.5f + x1 * scale;
  sy = height() * 0.5f - y2 * scale;
  depth = z2;
}

ViewCubeWidget::FaceId ViewCubeWidget::hit_test(const QPoint& pos) const {
  static const FaceGeom faces[] = {
      {FaceId::Right, 'r', "RIGHT", 1, 0, 0, QColor(210, 90, 90)},
      {FaceId::Left, 'l', "LEFT", -1, 0, 0, QColor(210, 90, 90)},
      {FaceId::Top, 't', "TOP", 0, 1, 0, QColor(90, 190, 110)},
      {FaceId::Bottom, 'b', "BOTTOM", 0, -1, 0, QColor(90, 190, 110)},
      {FaceId::Front, 'f', "FRONT", 0, 0, 1, QColor(90, 140, 220)},
      {FaceId::Back, 'k', "BACK", 0, 0, -1, QColor(90, 140, 220)},
  };

  FaceId best = FaceId::None;
  float best_depth = -1e9f;
  for (const FaceGeom& f : faces) {
    // Face quad corners in world (unit cube).
    float u[3]{0, 0, 0};
    float v[3]{0, 0, 0};
    if (f.nx != 0) {
      u[1] = 1;
      v[2] = 1;
    } else if (f.ny != 0) {
      u[0] = 1;
      v[2] = 1;
    } else {
      u[0] = 1;
      v[1] = 1;
    }

    QPolygonF poly;
    float depth_sum = 0.0f;
    for (int i = 0; i < 4; ++i) {
      const float su = (i == 0 || i == 3) ? -1.0f : 1.0f;
      const float sv = (i < 2) ? -1.0f : 1.0f;
      const float px = f.nx + u[0] * su + v[0] * sv;
      const float py = f.ny + u[1] * su + v[1] * sv;
      const float pz = f.nz + u[2] * su + v[2] * sv;
      float sx, sy, d;
      project_point(px * 0.7f, py * 0.7f, pz * 0.7f, sx, sy, d);
      poly << QPointF(sx, sy);
      depth_sum += d;
    }
    const float depth = depth_sum * 0.25f;
    // Face visible if its outward normal points toward camera (approx via depth
    // of face center vs origin).
    float cx, cy, cd;
    project_point(f.nx * 0.7f, f.ny * 0.7f, f.nz * 0.7f, cx, cy, cd);
    float ox, oy, od;
    project_point(0, 0, 0, ox, oy, od);
    if (cd < od) continue;  // facing away
    if (poly.containsPoint(pos, Qt::OddEvenFill) && cd > best_depth) {
      best_depth = cd;
      best = f.id;
    }
  }

  // Home button (small circle bottom-center of widget).
  const QPointF home_c(width() * 0.5, height() - 14.0);
  if (QLineF(home_c, pos).length() <= 10.0) return FaceId::Home;
  return best;
}

void ViewCubeWidget::paintEvent(QPaintEvent*) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing, true);

  // Soft plate behind the cube.
  p.setPen(Qt::NoPen);
  p.setBrush(QColor(20, 22, 26, 160));
  p.drawRoundedRect(rect().adjusted(2, 2, -2, -2), 12, 12);

  static const FaceGeom faces[] = {
      {FaceId::Right, 'r', "RIGHT", 1, 0, 0, QColor(196, 86, 86)},
      {FaceId::Left, 'l', "LEFT", -1, 0, 0, QColor(176, 76, 76)},
      {FaceId::Top, 't', "TOP", 0, 1, 0, QColor(86, 170, 104)},
      {FaceId::Bottom, 'b', "BOTTOM", 0, -1, 0, QColor(70, 150, 90)},
      {FaceId::Front, 'f', "FRONT", 0, 0, 1, QColor(86, 130, 200)},
      {FaceId::Back, 'k', "BACK", 0, 0, -1, QColor(70, 110, 180)},
  };

  std::vector<ProjFace> projected;
  projected.reserve(6);
  for (const FaceGeom& f : faces) {
    float u[3]{0, 0, 0};
    float v[3]{0, 0, 0};
    if (f.nx != 0) {
      u[1] = 1;
      v[2] = 1;
    } else if (f.ny != 0) {
      u[0] = 1;
      v[2] = 1;
    } else {
      u[0] = 1;
      v[1] = 1;
    }

    ProjFace pf;
    pf.id = f.id;
    pf.code = f.code;
    pf.label = f.label;
    pf.color = f.color;
    float depth_sum = 0.0f;
    for (int i = 0; i < 4; ++i) {
      const float su = (i == 0 || i == 3) ? -1.0f : 1.0f;
      const float sv = (i < 2) ? -1.0f : 1.0f;
      const float px = f.nx + u[0] * su + v[0] * sv;
      const float py = f.ny + u[1] * su + v[1] * sv;
      const float pz = f.nz + u[2] * su + v[2] * sv;
      float sx, sy, d;
      project_point(px * 0.70f, py * 0.70f, pz * 0.70f, sx, sy, d);
      pf.poly << QPointF(sx, sy);
      depth_sum += d;
    }
    pf.depth = depth_sum * 0.25f;
    float cx, cy, cd, ox, oy, od;
    project_point(f.nx * 0.70f, f.ny * 0.70f, f.nz * 0.70f, cx, cy, cd);
    project_point(0, 0, 0, ox, oy, od);
    pf.visible = cd >= od - 1e-3f;
    if (pf.visible) projected.push_back(pf);
  }

  std::sort(projected.begin(), projected.end(),
            [](const ProjFace& a, const ProjFace& b) { return a.depth < b.depth; });

  for (const ProjFace& pf : projected) {
    QColor fill = pf.color;
    if (pf.id == hover_) fill = fill.lighter(125);
    p.setBrush(fill);
    p.setPen(QPen(QColor(245, 245, 245, 220), 1.2));
    p.drawPolygon(pf.poly);

    const QPointF c = pf.poly.boundingRect().center();
    p.setPen(QColor(255, 255, 255));
    QFont font = p.font();
    font.setPointSize(7);
    font.setBold(true);
    p.setFont(font);
    p.drawText(QRectF(c.x() - 24, c.y() - 8, 48, 16), Qt::AlignCenter,
               QString::fromUtf8(pf.label));
  }

  // World-axis triad near the cube (screen-space from projected unit axes).
  struct Axis {
    float x, y, z;
    QColor color;
    const char* label;
  };
  const Axis axes[] = {
      {1, 0, 0, QColor(230, 70, 70), "X"},
      {0, 1, 0, QColor(70, 200, 90), "Y"},
      {0, 0, 1, QColor(70, 130, 230), "Z"},
  };
  float ox, oy, od;
  project_point(0, 0, 0, ox, oy, od);
  for (const Axis& a : axes) {
    float sx, sy, sd;
    project_point(a.x * 1.05f, a.y * 1.05f, a.z * 1.05f, sx, sy, sd);
    p.setPen(QPen(a.color, 2.2));
    p.drawLine(QPointF(ox, oy), QPointF(sx, sy));
    p.setPen(a.color);
    p.drawText(QPointF(sx + 3, sy - 2), QString::fromUtf8(a.label));
  }

  // Home chip.
  const bool home_hover = hover_ == FaceId::Home;
  p.setBrush(home_hover ? QColor(240, 200, 80) : QColor(200, 200, 200));
  p.setPen(QPen(QColor(40, 40, 40), 1));
  p.drawEllipse(QPointF(width() * 0.5, height() - 14.0), 9, 9);
  p.setPen(QColor(30, 30, 30));
  QFont hf = p.font();
  hf.setPointSize(6);
  hf.setBold(true);
  p.setFont(hf);
  p.drawText(QRectF(width() * 0.5 - 10, height() - 20, 20, 14), Qt::AlignCenter,
             QStringLiteral("H"));
}

void ViewCubeWidget::mousePressEvent(QMouseEvent* event) {
  if (!camera_ || event->button() != Qt::LeftButton) return;
  const FaceId id = hit_test(event->position().toPoint());
  char code = 0;
  switch (id) {
    case FaceId::Right:
      code = 'r';
      break;
    case FaceId::Left:
      code = 'l';
      break;
    case FaceId::Top:
      code = 't';
      break;
    case FaceId::Bottom:
      code = 'b';
      break;
    case FaceId::Front:
      code = 'f';
      break;
    case FaceId::Back:
      code = 'k';
      break;
    case FaceId::Home:
      code = 'h';
      break;
    default:
      break;
  }
  if (code != 0) {
    camera_->set_standard_view(code);
    notify_redraw();
    update();
  }
}

void ViewCubeWidget::mouseMoveEvent(QMouseEvent* event) {
  const FaceId id = hit_test(event->position().toPoint());
  if (id != hover_) {
    hover_ = id;
    update();
  }
}

void ViewCubeWidget::leaveEvent(QEvent*) {
  if (hover_ != FaceId::None) {
    hover_ = FaceId::None;
    update();
  }
}

void ViewCubeWidget::notify_redraw() {
  if (redraw_) redraw_();
}

}  // namespace brep::viewer
