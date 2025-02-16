#ifndef GIMBALMOTIONMODEBASE_H
#define GIMBALMOTIONMODEBASE_H

#include <QObject>

// Forward declare GimbalController
class GimbalController;

class GimbalMotionModeBase : public QObject
{
    Q_OBJECT
public:
    explicit GimbalMotionModeBase(QObject* parent = nullptr) : QObject(parent) {}
    virtual ~GimbalMotionModeBase() {}

    // Called when we enter this mode
    virtual void enterMode(GimbalController* controller) {}

    // Called when we exit this mode
    virtual void exitMode(GimbalController* controller) {}

    // Called periodically (e.g. from GimbalController::update())
    virtual void update(GimbalController* controller) {}
};


#endif // GIMBALMOTIONMODEBASE_H
